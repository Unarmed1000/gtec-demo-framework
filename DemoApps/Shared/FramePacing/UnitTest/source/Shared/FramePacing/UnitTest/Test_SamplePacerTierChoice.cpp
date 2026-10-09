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

  constexpr SamplePacerCapabilities Has(const bool vblankTimes, const bool waitForPresent, const bool presentAtTime = false,
                                        const bool presentSkipsOverdue = false) noexcept
  {
    SamplePacerCapabilities has;
    // What a tier is not made of, so it is never part of what a run uses
    has.PresentSwapIntervalMax = 4;
    has.PresentAfterDuration = true;
    has.DisplayTimes = true;
    has.WaitForGpuWork = true;
    has.GpuWorkTimes = true;
    has.GpuWorkDurations = true;
    has.VBlankTimes = vblankTimes;
    has.WaitForPresent = waitForPresent;
    has.PresentAtTime = presentAtTime;
    has.PresentSkipsOverdue = presentSkipsOverdue;
    return has;
  }

  constexpr SamplePacerCapabilities Uses(const bool vblankTimes, const bool waitForPresent, const bool presentAtTime = false,
                                         const bool presentSkipsOverdue = false) noexcept
  {
    SamplePacerCapabilities uses;
    uses.VBlankTimes = vblankTimes;
    uses.WaitForPresent = waitForPresent;
    uses.PresentAtTime = presentAtTime;
    uses.PresentSkipsOverdue = presentSkipsOverdue;
    return uses;
  }
}


TEST(TestSamplePacerTierChoice, Tiers_TheBestFirstAndEachOnce)
{
  const auto& tiers = SamplePacerTierChoiceUtil::Tiers;
  ASSERT_EQ(12u, tiers.size());
  EXPECT_EQ(SamplePacerTier::TimedSkipVBlankWaitForPresent, tiers[0]);
  EXPECT_EQ(SamplePacerTier::TimedSkipTimerPeriodOnly, tiers[3]);
  EXPECT_EQ(SamplePacerTier::TimedVBlankWaitForPresent, tiers[4]);
  EXPECT_EQ(SamplePacerTier::TimedTimerPeriodOnly, tiers[7]);
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


TEST(TestSamplePacerTierChoice, ToKind_EveryTierHasTheKindOfItsSubTier)
{
  EXPECT_EQ(SamplePacerKind::VBlankWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::VBlankWaitForPresent));
  EXPECT_EQ(SamplePacerKind::VBlankPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::VBlankPeriodOnly));
  EXPECT_EQ(SamplePacerKind::TimerWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimerWaitForPresent));
  EXPECT_EQ(SamplePacerKind::TimerPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimerPeriodOnly));
  // The same four where the display places the frame
  EXPECT_EQ(SamplePacerKind::VBlankWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimedVBlankWaitForPresent));
  EXPECT_EQ(SamplePacerKind::VBlankPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimedVBlankPeriodOnly));
  EXPECT_EQ(SamplePacerKind::TimerWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimedTimerWaitForPresent));
  EXPECT_EQ(SamplePacerKind::TimerPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimedTimerPeriodOnly));
  EXPECT_EQ(SamplePacerKind::VBlankWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimedSkipVBlankWaitForPresent));
  EXPECT_EQ(SamplePacerKind::VBlankPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimedSkipVBlankPeriodOnly));
  EXPECT_EQ(SamplePacerKind::TimerWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimedSkipTimerWaitForPresent));
  EXPECT_EQ(SamplePacerKind::TimerPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimedSkipTimerPeriodOnly));
}


TEST(TestSamplePacerTierChoice, ToLoopPlacedTier_IsTheTierOfTheKindInTheLastMajorTier)
{
  for (const SamplePacerTier tier : SamplePacerTierChoiceUtil::Tiers)
  {
    const SamplePacerTier loopPlacedTier = SamplePacerTierChoiceUtil::ToLoopPlacedTier(SamplePacerTierChoiceUtil::ToKind(tier));
    EXPECT_FALSE(SamplePacerTierChoiceUtil::IsPresentAtTimeTier(loopPlacedTier));
    EXPECT_EQ(SamplePacerTierChoiceUtil::ToKind(tier), SamplePacerTierChoiceUtil::ToKind(loopPlacedTier));
    if (!SamplePacerTierChoiceUtil::IsPresentAtTimeTier(tier))
    {
      EXPECT_EQ(tier, loopPlacedTier);
    }
  }
}


TEST(TestSamplePacerTierChoice, IsPresentAtTimeTier_TheFirstTwoMajorTiers)
{
  for (std::size_t i = 0; i < SamplePacerTierChoiceUtil::Tiers.size(); ++i)
  {
    EXPECT_EQ(i < 8u, SamplePacerTierChoiceUtil::IsPresentAtTimeTier(SamplePacerTierChoiceUtil::Tiers[i]));
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
  EXPECT_EQ(Uses(true, true), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true), SamplePacerKind::VBlankWaitForPresent, false));
  EXPECT_EQ(Uses(true, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true), SamplePacerKind::VBlankPeriodOnly, false));
  EXPECT_EQ(Uses(false, true), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true), SamplePacerKind::TimerWaitForPresent, false));
  EXPECT_EQ(Uses(false, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true), SamplePacerKind::TimerPeriodOnly, false));

  // The app has one of them, or none
  EXPECT_EQ(Uses(true, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, false), SamplePacerKind::VBlankWaitForPresent, false));
  EXPECT_EQ(Uses(false, true), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(false, true), SamplePacerKind::VBlankWaitForPresent, false));
  EXPECT_EQ(Uses(false, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(false, false), SamplePacerKind::VBlankWaitForPresent, false));
  EXPECT_EQ(Uses(false, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(false, true), SamplePacerKind::VBlankPeriodOnly, false));
  EXPECT_EQ(Uses(false, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, false), SamplePacerKind::TimerWaitForPresent, false));
}


TEST(TestSamplePacerTierChoice, ToUsedCapabilities_TheTimeOnThePresentWhereItIsAskedForAndTheAppHasIt)
{
  // Asked for and there
  EXPECT_EQ(Uses(true, false, true), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true, true), SamplePacerKind::VBlankPeriodOnly, true));
  // Not asked for, or not there
  EXPECT_EQ(Uses(true, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true, true), SamplePacerKind::VBlankPeriodOnly, false));
  EXPECT_EQ(Uses(true, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true, false), SamplePacerKind::VBlankPeriodOnly, true));
  // What the display does with a frame that is overdue comes with the time: a run does not choose it
  EXPECT_EQ(Uses(false, false, true, true),
            SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(false, false, true, true), SamplePacerKind::TimerPeriodOnly, true));
  EXPECT_EQ(Uses(false, false),
            SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(false, false, true, true), SamplePacerKind::TimerPeriodOnly, false));
  // And it is nothing without a time on the present
  EXPECT_EQ(Uses(false, false),
            SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(false, false, false, true), SamplePacerKind::TimerPeriodOnly, true));
}


TEST(TestSamplePacerTierChoice, RatedUse_ThePacerThatWasAskedForOrThePacerOfWhatIsLeft)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  const auto tierOf = [](const SamplePacerCapabilities& has, const SamplePacerKind kind, const bool presentAtTime = false)
  { return SamplePacer::Rate(SamplePacerTierChoiceUtil::ToUsedCapabilities(has, kind, presentAtTime)).Tier; };

  // A app with everything is in the tier that was asked for, whichever it is. A display that leaves out a frame that is overdue
  // is a fact of the system, so there are two such apps: one for the first major tier and one for the second.
  for (const SamplePacerTier tier : SamplePacerTierChoiceUtil::Tiers)
  {
    const bool isTimed = SamplePacerTierChoiceUtil::IsPresentAtTimeTier(tier);
    const bool isSkip = isTimed && SamplePacer::GetTierNumber(tier).starts_with("1.");
    EXPECT_EQ(tier, tierOf(Has(true, true, true, isSkip), SamplePacerTierChoiceUtil::ToKind(tier), isTimed));
  }
  // Without a time on the present: the tier where the frame loop places the frame, whatever was asked for
  EXPECT_EQ(SamplePacerTier::VBlankWaitForPresent, tierOf(Has(true, true), SamplePacerKind::VBlankWaitForPresent, true));
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, tierOf(Has(true, true), SamplePacerKind::TimerPeriodOnly, true));
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
