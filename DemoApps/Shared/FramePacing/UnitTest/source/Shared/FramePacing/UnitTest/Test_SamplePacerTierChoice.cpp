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


#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <Shared/FramePacing/SamplePacer.hpp>
#include <Shared/FramePacing/SamplePacerTierChoice.hpp>

using namespace Fsl;

namespace
{
  using TestSamplePacerTierChoice = TestFixtureFslBase;

  struct Set
  {
    bool VBlankTimes{false};
    bool WaitForPresent{false};
    bool PresentAtTime{false};
    bool PresentAfterDuration{false};
    bool DisplayTimes{false};
    bool PresentSkipsOverdue{false};
  };

  constexpr SamplePacerCapabilities Has(const Set& set) noexcept
  {
    SamplePacerCapabilities has;
    // What a tier is not made of, so it is never part of what a run uses
    has.WaitForGpuWork = true;
    has.GpuWorkTimes = true;
    has.GpuWorkDurations = true;
    has.VBlankTimes = set.VBlankTimes;
    has.WaitForPresent = set.WaitForPresent;
    has.PresentAtTime = set.PresentAtTime;
    has.PresentAfterDuration = set.PresentAfterDuration;
    has.DisplayTimes = set.DisplayTimes;
    has.PresentSkipsOverdue = set.PresentSkipsOverdue;
    return has;
  }

  constexpr SamplePacerCapabilities Uses(const Set& set) noexcept
  {
    SamplePacerCapabilities uses;
    uses.VBlankTimes = set.VBlankTimes;
    uses.WaitForPresent = set.WaitForPresent;
    uses.PresentAtTime = set.PresentAtTime;
    uses.PresentAfterDuration = set.PresentAfterDuration;
    uses.DisplayTimes = set.DisplayTimes;
    uses.PresentSkipsOverdue = set.PresentSkipsOverdue;
    return uses;
  }

  constexpr SamplePacerCapabilities Has(const bool vblankTimes, const bool waitForPresent) noexcept
  {
    return Has(Set{vblankTimes, waitForPresent});
  }

  constexpr SamplePacerCapabilities Uses(const bool vblankTimes, const bool waitForPresent) noexcept
  {
    return Uses(Set{vblankTimes, waitForPresent});
  }

  constexpr SamplePacerCapabilities UsedOf(const SamplePacerCapabilities& has, const SamplePacerKind kind) noexcept
  {
    return SamplePacerTierChoiceUtil::ToUsedCapabilities(has, kind, false, false, false);
  }
}


TEST(TestSamplePacerTierChoice, Tiers_TheBestFirstAndEachOnce)
{
  const auto& tiers = SamplePacerTierChoiceUtil::Tiers;
  ASSERT_EQ(12u, tiers.size());
  EXPECT_EQ(SamplePacerTier::AtTimeVBlankWaitForPresent, tiers[0]);
  EXPECT_EQ(SamplePacerTier::AtTimeTimerPeriodOnly, tiers[3]);
  EXPECT_EQ(SamplePacerTier::AfterDurationVBlankWaitForPresent, tiers[4]);
  EXPECT_EQ(SamplePacerTier::AfterDurationTimerDisplayTimes, tiers[7]);
  EXPECT_EQ(SamplePacerTier::VBlankWaitForPresent, tiers[8]);
  EXPECT_EQ(SamplePacerTier::VBlankPeriodOnly, tiers[9]);
  EXPECT_EQ(SamplePacerTier::TimerWaitForPresent, tiers[10]);
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, tiers[11]);
  for (std::size_t i = 0; i < tiers.size(); ++i)
  {
    for (std::size_t j = i + 1u; j < tiers.size(); ++j)
    {
      EXPECT_NE(tiers[i], tiers[j]);
    }
  }
}


TEST(TestSamplePacerTierChoice, Tiers_AreInTheOrderTheLibraryNumbersThem)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  const std::array<std::string_view, 12> numbers = {"1.1", "1.2", "1.3", "1.4", "2.1", "2.2", "2.3", "2.4", "3.1", "3.2", "3.3", "3.4"};
  for (std::size_t i = 0; i < numbers.size(); ++i)
  {
    EXPECT_EQ(numbers[i], SamplePacer::GetTierNumber(SamplePacerTierChoiceUtil::Tiers[i]));
  }
}


TEST(TestSamplePacerTierChoice, SubTierKinds_TheSameInEveryMajorTier)
{
  // The side bar has a group for the major tier and one for the sub tier: a sub tier is what the frame loop paces with in every one
  for (std::size_t major = 0; major < SamplePacerTierChoiceUtil::MajorTierCount; ++major)
  {
    for (std::size_t sub = 0; sub < SamplePacerTierChoiceUtil::SubTierCount; ++sub)
    {
      const SamplePacerTier tier = SamplePacerTierChoiceUtil::Tiers[(major * SamplePacerTierChoiceUtil::SubTierCount) + sub];
      EXPECT_EQ(SamplePacerTierChoiceUtil::SubTierKinds[sub], SamplePacerTierChoiceUtil::ToKind(tier));
    }
  }
}


TEST(TestSamplePacerTierChoice, ToTierIndex_IsThePlaceInTiers)
{
  for (std::size_t i = 0; i < SamplePacerTierChoiceUtil::Tiers.size(); ++i)
  {
    EXPECT_EQ(i, SamplePacerTierChoiceUtil::ToTierIndex(SamplePacerTierChoiceUtil::Tiers[i]));
  }
}


TEST(TestSamplePacerTierChoice, ToKind_EveryTierHasTheKindOfItsSubTier)
{
  EXPECT_EQ(SamplePacerKind::VBlankWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::VBlankWaitForPresent));
  EXPECT_EQ(SamplePacerKind::VBlankPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::VBlankPeriodOnly));
  EXPECT_EQ(SamplePacerKind::TimerWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimerWaitForPresent));
  EXPECT_EQ(SamplePacerKind::TimerPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimerPeriodOnly));
  // The same four where the display places the frame
  EXPECT_EQ(SamplePacerKind::VBlankWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::AtTimeVBlankWaitForPresent));
  EXPECT_EQ(SamplePacerKind::VBlankPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::AtTimeVBlankPeriodOnly));
  EXPECT_EQ(SamplePacerKind::TimerWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::AtTimeTimerWaitForPresent));
  EXPECT_EQ(SamplePacerKind::TimerPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::AtTimeTimerPeriodOnly));
  EXPECT_EQ(SamplePacerKind::VBlankWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::AfterDurationVBlankWaitForPresent));
  EXPECT_EQ(SamplePacerKind::VBlankPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::AfterDurationVBlankDisplayTimes));
  EXPECT_EQ(SamplePacerKind::TimerWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::AfterDurationTimerWaitForPresent));
  EXPECT_EQ(SamplePacerKind::TimerPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::AfterDurationTimerDisplayTimes));
}


TEST(TestSamplePacerTierChoice, ToLoopPlacedTier_IsTheTierOfTheKindInTheLastMajorTier)
{
  for (const SamplePacerTier tier : SamplePacerTierChoiceUtil::Tiers)
  {
    const SamplePacerTier loopPlacedTier = SamplePacerTierChoiceUtil::ToLoopPlacedTier(SamplePacerTierChoiceUtil::ToKind(tier));
    EXPECT_FALSE(SamplePacerTierChoiceUtil::IsPresentAtTimeTier(loopPlacedTier));
    EXPECT_FALSE(SamplePacerTierChoiceUtil::IsPresentAfterDurationTier(loopPlacedTier));
    EXPECT_EQ(SamplePacerTierChoiceUtil::ToKind(tier), SamplePacerTierChoiceUtil::ToKind(loopPlacedTier));
    if (!SamplePacerTierChoiceUtil::IsPresentAtTimeTier(tier) && !SamplePacerTierChoiceUtil::IsPresentAfterDurationTier(tier))
    {
      EXPECT_EQ(tier, loopPlacedTier);
    }
  }
}


TEST(TestSamplePacerTierChoice, IsPresentAtTimeTier_TheFirstMajorTier)
{
  for (std::size_t i = 0; i < SamplePacerTierChoiceUtil::Tiers.size(); ++i)
  {
    EXPECT_EQ(i < 4u, SamplePacerTierChoiceUtil::IsPresentAtTimeTier(SamplePacerTierChoiceUtil::Tiers[i]));
  }
}


TEST(TestSamplePacerTierChoice, IsPresentAfterDurationTier_TheSecondMajorTier)
{
  for (std::size_t i = 0; i < SamplePacerTierChoiceUtil::Tiers.size(); ++i)
  {
    EXPECT_EQ(i >= 4u && i < 8u, SamplePacerTierChoiceUtil::IsPresentAfterDurationTier(SamplePacerTierChoiceUtil::Tiers[i]));
  }
}


TEST(TestSamplePacerTierChoice, Kinds_WhatAPacerUses)
{
  EXPECT_TRUE(SamplePacerTierChoiceUtil::IsVBlankKind(SamplePacerKind::VBlankWaitForPresent));
  EXPECT_TRUE(SamplePacerTierChoiceUtil::IsVBlankKind(SamplePacerKind::VBlankPeriodOnly));
  EXPECT_FALSE(SamplePacerTierChoiceUtil::IsVBlankKind(SamplePacerKind::TimerWaitForPresent));
  EXPECT_FALSE(SamplePacerTierChoiceUtil::IsVBlankKind(SamplePacerKind::TimerPeriodOnly));

  EXPECT_TRUE(SamplePacerTierChoiceUtil::IsPresentWaitKind(SamplePacerKind::VBlankWaitForPresent));
  EXPECT_FALSE(SamplePacerTierChoiceUtil::IsPresentWaitKind(SamplePacerKind::VBlankPeriodOnly));
  EXPECT_TRUE(SamplePacerTierChoiceUtil::IsPresentWaitKind(SamplePacerKind::TimerWaitForPresent));
  EXPECT_FALSE(SamplePacerTierChoiceUtil::IsPresentWaitKind(SamplePacerKind::TimerPeriodOnly));
}


TEST(TestSamplePacerTierChoice, ToUsedCapabilities_OnlyWhatThePacerUsesAndTheAppHas)
{
  // The app has both
  EXPECT_EQ(Uses(true, true), UsedOf(Has(true, true), SamplePacerKind::VBlankWaitForPresent));
  EXPECT_EQ(Uses(true, false), UsedOf(Has(true, true), SamplePacerKind::VBlankPeriodOnly));
  EXPECT_EQ(Uses(false, true), UsedOf(Has(true, true), SamplePacerKind::TimerWaitForPresent));
  EXPECT_EQ(Uses(false, false), UsedOf(Has(true, true), SamplePacerKind::TimerPeriodOnly));

  // The app has one of them, or none
  EXPECT_EQ(Uses(true, false), UsedOf(Has(true, false), SamplePacerKind::VBlankWaitForPresent));
  EXPECT_EQ(Uses(false, true), UsedOf(Has(false, true), SamplePacerKind::VBlankWaitForPresent));
  EXPECT_EQ(Uses(false, false), UsedOf(Has(false, false), SamplePacerKind::VBlankWaitForPresent));
  EXPECT_EQ(Uses(false, false), UsedOf(Has(false, true), SamplePacerKind::VBlankPeriodOnly));
  EXPECT_EQ(Uses(false, false), UsedOf(Has(true, false), SamplePacerKind::TimerWaitForPresent));
}


TEST(TestSamplePacerTierChoice, ToUsedCapabilities_TheTimesOnThePresentWhereTheyAreAskedForAndTheAppHasThem)
{
  const SamplePacerCapabilities has = Has(Set{true, true, true, true});
  const SamplePacerKind kind = SamplePacerKind::VBlankPeriodOnly;
  // The time before which the frame is not shown, asked for and there
  EXPECT_EQ(Uses(Set{true, false, true}), SamplePacerTierChoiceUtil::ToUsedCapabilities(has, kind, true, false, false));
  // A present takes one of the two times: with both asked for it is that one
  EXPECT_EQ(Uses(Set{true, false, true}), SamplePacerTierChoiceUtil::ToUsedCapabilities(has, kind, true, true, false));
  // The time the frame before stays on screen
  EXPECT_EQ(Uses(Set{true, false, false, true}), SamplePacerTierChoiceUtil::ToUsedCapabilities(has, kind, false, true, false));
  // Not asked for, or not there
  EXPECT_EQ(Uses(true, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(has, kind, false, false, false));
  EXPECT_EQ(Uses(true, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true), kind, true, true, false));
}


TEST(TestSamplePacerTierChoice, ToUsedCapabilities_TheDisplayTimesWhereTheRunGivesThem)
{
  const SamplePacerCapabilities has = Has(Set{false, false, false, false, true});
  EXPECT_EQ(Uses(Set{false, false, false, false, true}),
            SamplePacerTierChoiceUtil::ToUsedCapabilities(has, SamplePacerKind::TimerPeriodOnly, false, false, true));
  EXPECT_EQ(Uses(false, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(has, SamplePacerKind::TimerPeriodOnly, false, false, false));
}


TEST(TestSamplePacerTierChoice, ToUsedCapabilities_WhatTheDisplayDoesWithAFrameThatIsOverdueIsAFactOfTheSystem)
{
  // A run does not choose it, and it comes with or without a time on the present
  const SamplePacerCapabilities has = Has(Set{false, false, true, false, false, true});
  EXPECT_EQ(Uses(Set{false, false, true, false, false, true}),
            SamplePacerTierChoiceUtil::ToUsedCapabilities(has, SamplePacerKind::TimerPeriodOnly, true, false, false));
  EXPECT_EQ(Uses(Set{false, false, false, false, false, true}), UsedOf(has, SamplePacerKind::TimerPeriodOnly));
}


TEST(TestSamplePacerTierChoice, RatedUse_ThePacerThatWasAskedForOrThePacerOfWhatIsLeft)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  const auto tierOf = [](const SamplePacerCapabilities& has, const SamplePacerKind kind, const bool presentAtTime = false,
                         const bool presentAfterDuration = false, const bool displayTimes = false)
  { return SamplePacer::Rate(SamplePacerTierChoiceUtil::ToUsedCapabilities(has, kind, presentAtTime, presentAfterDuration, displayTimes)).Tier; };

  // A app with everything is in the tier that was asked for, whichever it is
  const SamplePacerCapabilities everything = Has(Set{true, true, true, true, true});
  for (const SamplePacerTier tier : SamplePacerTierChoiceUtil::Tiers)
  {
    EXPECT_EQ(tier, tierOf(everything, SamplePacerTierChoiceUtil::ToKind(tier), SamplePacerTierChoiceUtil::IsPresentAtTimeTier(tier),
                           SamplePacerTierChoiceUtil::IsPresentAfterDurationTier(tier), true));
  }
  // Without a time on the present: the tier where the frame loop places the frame, whatever was asked for
  EXPECT_EQ(SamplePacerTier::VBlankWaitForPresent, tierOf(Has(true, true), SamplePacerKind::VBlankWaitForPresent, true, true));
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, tierOf(Has(true, true), SamplePacerKind::TimerPeriodOnly, true, true));
  // A duration on the present without a wait for a present or display times: the frame loop places the frame
  EXPECT_EQ(SamplePacerTier::VBlankPeriodOnly, tierOf(Has(Set{true, false, false, true}), SamplePacerKind::VBlankPeriodOnly, false, true));
  // With the display times instead of the wait
  EXPECT_EQ(SamplePacerTier::AfterDurationVBlankDisplayTimes,
            tierOf(Has(Set{true, false, false, true, true}), SamplePacerKind::VBlankWaitForPresent, false, true, true));
  // Without a present to wait for: the pacer of the vertical blank times alone, and a timer that waits is a timer
  EXPECT_EQ(SamplePacerTier::VBlankPeriodOnly, tierOf(Has(true, false), SamplePacerKind::VBlankWaitForPresent));
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, tierOf(Has(true, false), SamplePacerKind::TimerWaitForPresent));
  // Without vertical blank times: the timer, with the wait where that was asked for
  EXPECT_EQ(SamplePacerTier::TimerWaitForPresent, tierOf(Has(false, true), SamplePacerKind::VBlankWaitForPresent));
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, tierOf(Has(false, true), SamplePacerKind::VBlankPeriodOnly));
  // Without both: the baseline
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, tierOf(Has(false, false), SamplePacerKind::VBlankWaitForPresent));
}


TEST(TestSamplePacerTierChoice, KindChangeOrder_HasEveryChangeFromOneKindToAnotherOnce)
{
  const auto& order = SamplePacerTierChoiceUtil::KindChangeOrder;
  for (const SamplePacerTier fromTier : SamplePacerTierChoiceUtil::Tiers)
  {
    for (const SamplePacerTier toTier : SamplePacerTierChoiceUtil::Tiers)
    {
      const SamplePacerKind from = SamplePacerTierChoiceUtil::ToKind(fromTier);
      const SamplePacerKind to = SamplePacerTierChoiceUtil::ToKind(toTier);
      uint32_t count = 0;
      for (std::size_t i = 0; i < order.size(); ++i)
      {
        // Around to the first again
        if (order[i] == from && order[(i + 1u) % order.size()] == to)
        {
          ++count;
        }
      }
      EXPECT_EQ(from == to ? 0u : 1u, count);
    }
  }
}


TEST(TestSamplePacerTierChoice, FindKindChangeIndex_IsAPlaceOfTheKind)
{
  for (const SamplePacerTier tier : SamplePacerTierChoiceUtil::Tiers)
  {
    const SamplePacerKind kind = SamplePacerTierChoiceUtil::ToKind(tier);
    EXPECT_EQ(kind, SamplePacerTierChoiceUtil::KindChangeOrder[SamplePacerTierChoiceUtil::FindKindChangeIndex(kind)]);
  }
}
