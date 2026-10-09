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

  constexpr SamplePacerCapabilities Has(const bool vblankTimes, const bool waitForPresent) noexcept
  {
    SamplePacerCapabilities has;
    // What the choice of a kind does not use, so it is never part of what a run uses
    has.PresentSwapIntervalMax = 4;
    has.PresentAfterDuration = true;
    has.DisplayTimes = true;
    has.VBlankTimes = vblankTimes;
    has.WaitForPresent = waitForPresent;
    return has;
  }

  constexpr SamplePacerCapabilities Uses(const bool vblankTimes, const bool waitForPresent) noexcept
  {
    SamplePacerCapabilities uses;
    uses.VBlankTimes = vblankTimes;
    uses.WaitForPresent = waitForPresent;
    return uses;
  }
}


TEST(TestSamplePacerTierChoice, Tiers_TheBestFirstAndEachOnce)
{
  EXPECT_EQ(SamplePacerTier::VBlankWaitForPresent, SamplePacerTierChoiceUtil::Tiers[0]);
  EXPECT_EQ(SamplePacerTier::VBlankPeriodOnly, SamplePacerTierChoiceUtil::Tiers[1]);
  EXPECT_EQ(SamplePacerTier::TimerWaitForPresent, SamplePacerTierChoiceUtil::Tiers[2]);
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, SamplePacerTierChoiceUtil::Tiers[3]);
}


TEST(TestSamplePacerTierChoice, ToKind_EveryTierHasItsKind)
{
  EXPECT_EQ(SamplePacerKind::VBlankWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::VBlankWaitForPresent));
  EXPECT_EQ(SamplePacerKind::VBlankPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::VBlankPeriodOnly));
  EXPECT_EQ(SamplePacerKind::TimerWaitForPresent, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimerWaitForPresent));
  EXPECT_EQ(SamplePacerKind::TimerPeriodOnly, SamplePacerTierChoiceUtil::ToKind(SamplePacerTier::TimerPeriodOnly));
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
  EXPECT_EQ(Uses(true, true), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true), SamplePacerKind::VBlankWaitForPresent));
  EXPECT_EQ(Uses(true, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true), SamplePacerKind::VBlankPeriodOnly));
  EXPECT_EQ(Uses(false, true), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true), SamplePacerKind::TimerWaitForPresent));
  EXPECT_EQ(Uses(false, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, true), SamplePacerKind::TimerPeriodOnly));

  // The app has one of them, or none
  EXPECT_EQ(Uses(true, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, false), SamplePacerKind::VBlankWaitForPresent));
  EXPECT_EQ(Uses(false, true), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(false, true), SamplePacerKind::VBlankWaitForPresent));
  EXPECT_EQ(Uses(false, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(false, false), SamplePacerKind::VBlankWaitForPresent));
  EXPECT_EQ(Uses(false, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(false, true), SamplePacerKind::VBlankPeriodOnly));
  EXPECT_EQ(Uses(false, false), SamplePacerTierChoiceUtil::ToUsedCapabilities(Has(true, false), SamplePacerKind::TimerWaitForPresent));
}


TEST(TestSamplePacerTierChoice, RatedUse_ThePacerThatWasAskedForOrThePacerOfWhatIsLeft)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  const auto tierOf = [](const SamplePacerCapabilities& has, const SamplePacerKind kind)
  { return SamplePacer::Rate(SamplePacerTierChoiceUtil::ToUsedCapabilities(has, kind)).Tier; };

  // A app with both is in the tier of the pacer that was asked for, whichever it is
  for (const SamplePacerTier tier : SamplePacerTierChoiceUtil::Tiers)
  {
    EXPECT_EQ(tier, tierOf(Has(true, true), SamplePacerTierChoiceUtil::ToKind(tier)));
  }
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
