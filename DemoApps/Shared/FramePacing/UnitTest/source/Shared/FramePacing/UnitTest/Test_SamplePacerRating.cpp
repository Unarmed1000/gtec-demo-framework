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
#include <Shared/FramePacing/SamplePacerKind.hpp>
#include <Shared/FramePacing/SamplePacerRating.hpp>
#include <Shared/FramePacing/SamplePacerTierChoice.hpp>
#include <optional>

using namespace Fsl;

namespace
{
  using TestSamplePacerRating = TestFixtureFslBase;

  //! A app that has the vertical blank times and can wait for a present, so every kind paces as itself
  SamplePacerConfig Config(const SamplePacerKind kind)
  {
    SamplePacerConfig config;
    config.Kind = kind;
    config.Capabilities.VBlankTimes = true;
    config.Capabilities.WaitForPresent = true;
    return config;
  }
}

// The tiers and the rule that gives a tier are the pacer library's. These tests hold the sample's translation to and from its own
// types against the library, they do not define a tier.

TEST(TestSamplePacerRating, Rate_Baseline_IsTheLastTier)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  const SamplePacerRating rating = SamplePacer::Rate(SamplePacerCapabilities());

  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, rating.Tier);
  EXPECT_FALSE(rating.DisplaySideHolds);
  // The last sub tier of the last major tier
  EXPECT_EQ("3.4", SamplePacer::GetTierNumber(rating.Tier));
}


TEST(TestSamplePacerRating, Rate_FrameCallback_HoldsTheLoopAtTheTierOfAWaitForAPresent)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.FrameCallback = true;

  const SamplePacerRating rating = SamplePacer::Rate(capabilities);

  EXPECT_EQ(SamplePacerTier::TimerWaitForPresent, rating.Tier);
  EXPECT_TRUE(rating.HoldsByFrameCallback);
  EXPECT_EQ("3.3c", SamplePacer::GetRatingText(rating));
}


TEST(TestSamplePacerRating, Rate_FrameCallback_IsNotUsedNextToAWaitForAPresent)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.FrameCallback = true;
  capabilities.WaitForPresent = true;

  const SamplePacerRating rating = SamplePacer::Rate(capabilities);

  EXPECT_EQ(SamplePacerTier::TimerWaitForPresent, rating.Tier);
  EXPECT_FALSE(rating.HoldsByFrameCallback);
}


TEST(TestSamplePacerRating, Rate_PresentAfterDuration_TheDisplaySideHoldsAndTheTierIsTheSame)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.PresentAfterDuration = true;

  EXPECT_TRUE(SamplePacer::Rate(capabilities).DisplaySideHolds);
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, SamplePacer::Rate(capabilities).Tier);
}


TEST(TestSamplePacerRating, Rate_SwapIntervalOfOneAtMost_HoldsNothing)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.PresentSwapIntervalMax = 1;

  EXPECT_FALSE(SamplePacer::Rate(capabilities).DisplaySideHolds);
}


TEST(TestSamplePacerRating, Rate_SwapIntervalOfTwoOrMore_TheDisplaySideHolds)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.PresentSwapIntervalMax = 2;

  EXPECT_TRUE(SamplePacer::Rate(capabilities).DisplaySideHolds);
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, SamplePacer::Rate(capabilities).Tier);
}


TEST(TestSamplePacerRating, Rate_VBlankTimes)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.VBlankTimes = true;

  EXPECT_EQ(SamplePacerTier::VBlankPeriodOnly, SamplePacer::Rate(capabilities).Tier);
  EXPECT_FALSE(SamplePacer::Rate(capabilities).DisplaySideHolds);
}


TEST(TestSamplePacerRating, Rate_WaitForPresent)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.WaitForPresent = true;

  EXPECT_EQ(SamplePacerTier::TimerWaitForPresent, SamplePacer::Rate(capabilities).Tier);
}


TEST(TestSamplePacerRating, Rate_VBlankTimesAndWaitForPresent_IsTheBestTier)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.VBlankTimes = true;
  capabilities.WaitForPresent = true;

  EXPECT_EQ(SamplePacerTier::VBlankWaitForPresent, SamplePacer::Rate(capabilities).Tier);
  EXPECT_EQ("3.1", SamplePacer::GetTierNumber(SamplePacer::Rate(capabilities).Tier));
}


TEST(TestSamplePacerRating, Rate_DisplayTimes_AreNoTier)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.DisplayTimes = true;

  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, SamplePacer::Rate(capabilities).Tier);
  // They are the "+" of the rating
  EXPECT_TRUE(SamplePacer::Rate(capabilities).ReportsDisplayTimes);
}


TEST(TestSamplePacerRating, GetTierNumber_IsTheMajorTierAndTheSubTierOfTheLibrary)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  // The major tier where the frame loop places the frame
  EXPECT_EQ("3.1", SamplePacer::GetTierNumber(SamplePacerTier::VBlankWaitForPresent));
  EXPECT_EQ("3.2", SamplePacer::GetTierNumber(SamplePacerTier::VBlankPeriodOnly));
  EXPECT_EQ("3.3", SamplePacer::GetTierNumber(SamplePacerTier::TimerWaitForPresent));
  EXPECT_EQ("3.4", SamplePacer::GetTierNumber(SamplePacerTier::TimerPeriodOnly));
  // Where the display places the frame against a clock the order is the library's: what holds the loop first, then where the
  // refreshes are
  EXPECT_EQ("1.1", SamplePacer::GetTierNumber(SamplePacerTier::AtTimeVBlankWaitForPresent));
  EXPECT_EQ("1.2", SamplePacer::GetTierNumber(SamplePacerTier::AtTimeVBlankPeriodOnly));
  EXPECT_EQ("1.3", SamplePacer::GetTierNumber(SamplePacerTier::AtTimeTimerWaitForPresent));
  EXPECT_EQ("1.4", SamplePacer::GetTierNumber(SamplePacerTier::AtTimeTimerPeriodOnly));
  // And where it places the frame relative to the one before, where the display times take the place of the wait
  EXPECT_EQ("2.1", SamplePacer::GetTierNumber(SamplePacerTier::AfterDurationVBlankWaitForPresent));
  EXPECT_EQ("2.2", SamplePacer::GetTierNumber(SamplePacerTier::AfterDurationVBlankDisplayTimes));
  EXPECT_EQ("2.3", SamplePacer::GetTierNumber(SamplePacerTier::AfterDurationTimerWaitForPresent));
  EXPECT_EQ("2.4", SamplePacer::GetTierNumber(SamplePacerTier::AfterDurationTimerDisplayTimes));
}


TEST(TestSamplePacerRating, GetMajorTierNumber_IsTheFirstNumberOfTheTier)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  for (std::size_t i = 0; i < SamplePacerTierChoiceUtil::Tiers.size(); ++i)
  {
    const SamplePacerTier tier = SamplePacerTierChoiceUtil::Tiers[i];
    EXPECT_EQ(SamplePacer::GetTierNumber(tier).substr(0, 1), SamplePacer::GetMajorTierNumber(tier));
    EXPECT_FALSE(SamplePacer::GetMajorTierName(tier).empty());
    // The four of a major tier have its words, and the next major tier has other ones
    EXPECT_EQ(SamplePacer::GetMajorTierName(SamplePacerTierChoiceUtil::Tiers[(i / 4u) * 4u]), SamplePacer::GetMajorTierName(tier));
    if (i >= 4u)
    {
      EXPECT_NE(SamplePacer::GetMajorTierName(SamplePacerTierChoiceUtil::Tiers[i - 4u]), SamplePacer::GetMajorTierName(tier));
    }
  }
}


TEST(TestSamplePacerRating, TierTexts_EveryTierHasItsOwn)
{
  for (std::size_t i = 0; i < SamplePacerTierChoiceUtil::Tiers.size(); ++i)
  {
    for (std::size_t j = i + 1u; j < SamplePacerTierChoiceUtil::Tiers.size(); ++j)
    {
      const SamplePacerTier first = SamplePacerTierChoiceUtil::Tiers[i];
      const SamplePacerTier second = SamplePacerTierChoiceUtil::Tiers[j];
      EXPECT_NE(SamplePacer::GetTierLogName(first), SamplePacer::GetTierLogName(second));
      if (SamplePacer::IsSupported())
      {
        EXPECT_NE(SamplePacer::GetTierName(first), SamplePacer::GetTierName(second));
        EXPECT_NE(SamplePacer::GetTierDescription(first), SamplePacer::GetTierDescription(second));
        EXPECT_NE(SamplePacer::GetTierShortDescription(first), SamplePacer::GetTierShortDescription(second));
      }
    }
  }
}


TEST(TestSamplePacerRating, TierDescriptions_AreThoseOfTheLibrary)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  // The line the side bar shows
  const std::size_t maxLength = SamplePacer::GetTierShortDescriptionMaxLength();
  EXPECT_GT(maxLength, 0u);
  for (const SamplePacerTier tier : SamplePacerTierChoiceUtil::Tiers)
  {
    EXPECT_FALSE(SamplePacer::GetTierName(tier).empty());
    EXPECT_FALSE(SamplePacer::GetTierDescription(tier).empty());
    EXPECT_FALSE(SamplePacer::GetTierShortDescription(tier).empty());
    EXPECT_LE(SamplePacer::GetTierShortDescription(tier).size(), maxLength);
  }
}


TEST(TestSamplePacerRating, GetTier_EveryKindSaysItsTierOnceItHasAVerticalBlank)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  for (const SamplePacerTier tier : SamplePacerTierChoiceUtil::Tiers)
  {
    if (SamplePacerTierChoiceUtil::IsPresentAtTimeTier(tier) || SamplePacerTierChoiceUtil::IsPresentAfterDurationTier(tier))
    {
      continue;
    }
    SamplePacer pacer(Config(SamplePacerTierChoiceUtil::ToKind(tier)));
    // A kind that uses the vertical blank times paces on its clock until it was given one
    pacer.AddVBlank(NanosecondTickCount(1000000000), NanosecondTimeSpan(16666667), TickCount(10010000));

    EXPECT_EQ(tier, pacer.GetTier());
  }
}


TEST(TestSamplePacerRating, GetTier_WithATimeOnThePresentTheDisplayPlacesTheFrame)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.Capabilities.PresentAtTime = true;
  {
    const SamplePacer pacer(config);
    EXPECT_TRUE(pacer.IsPresentAtTimeInUse());
    EXPECT_EQ(SamplePacerTier::AtTimeTimerPeriodOnly, pacer.GetTier());
  }
  {    // Not asked for: the frame loop places the frame
    SamplePacerConfig loopConfig = config;
    loopConfig.PresentAtTime = false;
    const SamplePacer pacer(loopConfig);
    EXPECT_FALSE(pacer.IsPresentAtTimeInUse());
    EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, pacer.GetTier());
  }
  {    // A display that leaves out a frame that is overdue is a mark of the rating: the tier is the same
    SamplePacerConfig skipConfig = config;
    skipConfig.Capabilities.PresentSkipsOverdue = true;
    // What the run uses of what the app has: the timer, the time on the present and what the display does with a late frame
    const SamplePacerCapabilities uses = SamplePacerTierChoiceUtil::ToUsedCapabilities(skipConfig.Capabilities, skipConfig.Kind, true, false, false);
    EXPECT_EQ(SamplePacerTier::AtTimeTimerPeriodOnly, SamplePacer::Rate(uses).Tier);
    EXPECT_TRUE(SamplePacer::Rate(uses).SkipsOverdue);
    const SamplePacer pacer(skipConfig);
    EXPECT_EQ(SamplePacerTier::AtTimeTimerPeriodOnly, pacer.GetTier());
  }
}


// The second major tier has a pacer of its own: the run is in it and it paces it
TEST(TestSamplePacerRating, GetTier_WithADurationOnThePresentTheRunIsInTheSecondMajorTier)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerWaitForPresent);
  config.Capabilities.PresentAfterDuration = true;
  config.TimedPresent = true;
  const SamplePacer pacer(config);

  EXPECT_TRUE(pacer.IsTimedPresentInUse());
  EXPECT_EQ(SamplePacerTier::AfterDurationTimerWaitForPresent, pacer.GetTier());
  EXPECT_EQ(SamplePacerTier::AfterDurationTimerWaitForPresent, pacer.GetPacedAsTier());
}


TEST(TestSamplePacerRating, Rate_ADurationOnThePresent_NeedsAWaitForAPresentOrDisplayTimes)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.PresentAfterDuration = true;
  capabilities.VBlankTimes = true;
  // Neither: the frame loop places the frame
  EXPECT_EQ(SamplePacerTier::VBlankPeriodOnly, SamplePacer::Rate(capabilities).Tier);

  SamplePacerCapabilities withDisplayTimes = capabilities;
  withDisplayTimes.DisplayTimes = true;
  EXPECT_EQ(SamplePacerTier::AfterDurationVBlankDisplayTimes, SamplePacer::Rate(withDisplayTimes).Tier);
  EXPECT_TRUE(SamplePacer::Rate(withDisplayTimes).ReportsDisplayTimes);

  SamplePacerCapabilities withWait = capabilities;
  withWait.WaitForPresent = true;
  EXPECT_EQ(SamplePacerTier::AfterDurationVBlankWaitForPresent, SamplePacer::Rate(withWait).Tier);
  EXPECT_EQ("2.1", SamplePacer::GetTierNumber(SamplePacer::Rate(withWait).Tier));
}


TEST(TestSamplePacerRating, GetRatingText_TheNumberAndItsMarks)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  EXPECT_EQ("3.1", SamplePacer::GetRatingText({SamplePacerTier::VBlankWaitForPresent, false, false, false}));
  EXPECT_EQ("2.1+", SamplePacer::GetRatingText({SamplePacerTier::AfterDurationVBlankWaitForPresent, true, true, false}));
  EXPECT_EQ("3.1s", SamplePacer::GetRatingText({SamplePacerTier::VBlankWaitForPresent, false, false, true}));
  EXPECT_EQ("1.3+s", SamplePacer::GetRatingText({SamplePacerTier::AtTimeTimerWaitForPresent, true, true, true}));
}


TEST(TestSamplePacerRating, Rate_ATimeOnThePresent_IsTheFirstMajorTier)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.PresentAtTime = true;
  EXPECT_EQ(SamplePacerTier::AtTimeTimerPeriodOnly, SamplePacer::Rate(capabilities).Tier);
  EXPECT_TRUE(SamplePacer::Rate(capabilities).DisplaySideHolds);

  capabilities.VBlankTimes = true;
  capabilities.WaitForPresent = true;
  EXPECT_EQ(SamplePacerTier::AtTimeVBlankWaitForPresent, SamplePacer::Rate(capabilities).Tier);

  // What the display does with a frame that is overdue is a mark on any tier, and changes no tier
  SamplePacerCapabilities skipOnly;
  skipOnly.PresentSkipsOverdue = true;
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, SamplePacer::Rate(skipOnly).Tier);
  EXPECT_TRUE(SamplePacer::Rate(skipOnly).SkipsOverdue);
}


TEST(TestSamplePacerRating, Rate_WhatTheAppTellsOfTheGpuWork_ChangesNoTier)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.VBlankTimes = true;
  const SamplePacerRating rating = SamplePacer::Rate(capabilities);

  capabilities.GpuWorkTimes = true;
  capabilities.GpuWorkDurations = true;
  capabilities.WaitForGpuWork = true;
  EXPECT_EQ(rating, SamplePacer::Rate(capabilities));
}


TEST(TestSamplePacerRating, Rate_APresentThatTakesADuration_ChangesNoTier)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerCapabilities capabilities;
  capabilities.VBlankTimes = true;
  capabilities.PresentAfterDuration = true;
  const SamplePacerRating rating = SamplePacer::Rate(capabilities);

  // Only a present at a time lets the display place the frame. The duration holds a frame of two refreshes or more.
  EXPECT_EQ(SamplePacerTier::VBlankPeriodOnly, rating.Tier);
  EXPECT_TRUE(rating.DisplaySideHolds);
  EXPECT_EQ("3.2", SamplePacer::GetTierNumber(rating.Tier));
}
