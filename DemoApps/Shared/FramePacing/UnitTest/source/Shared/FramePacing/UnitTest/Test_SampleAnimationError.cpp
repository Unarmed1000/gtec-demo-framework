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

#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <Shared/FramePacing/SampleAnimationError.hpp>

using namespace Fsl;

namespace
{
  using TestSampleAnimationError = TestFixtureFslBase;

  //! A refresh of a 60 Hz display in ticks
  constexpr int64_t Refresh = 166667;

  //! Take frames until one that is judged
  bool TryPopJudged(SampleAnimationError& rAnimationError, SampleAnimationErrorRecord& rRecord)
  {
    while (rAnimationError.TryPop(rRecord))
    {
      if (rRecord.IsJudged)
      {
        return true;
      }
    }
    return false;
  }
}


TEST(TestSampleAnimationError, Empty_NothingToTake)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;

  EXPECT_FALSE(animationError.TryPop(record));
  EXPECT_EQ(SampleAnimationErrorStats(), animationError.CalcStats());
}


TEST(TestSampleAnimationError, AFrameWaitsForItsDisplayTime)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  animationError.AddFrame(1, TimeSpan(1000));

  EXPECT_FALSE(animationError.TryPop(record));
}


TEST(TestSampleAnimationError, TheFirstFrameIsNotJudged)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  animationError.AddFrame(1, TimeSpan(1000));
  animationError.AddDisplayTime(1, TickCount(500000));

  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(1u, record.PresentId);
  EXPECT_FALSE(record.IsJudged);
  EXPECT_EQ(TimeSpan(0), record.Error);
  EXPECT_FALSE(animationError.TryPop(record));
  EXPECT_EQ(0u, animationError.CalcStats().Frames);
}


TEST(TestSampleAnimationError, FramesShownAsFarApartAsTheyWereAnimated_NoError)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  animationError.AddFrame(1, TimeSpan(1000));
  animationError.AddFrame(2, TimeSpan(1000 + Refresh));
  animationError.AddDisplayTime(1, TickCount(500000));
  animationError.AddDisplayTime(2, TickCount(500000 + Refresh));

  ASSERT_TRUE(TryPopJudged(animationError, record));
  EXPECT_EQ(2u, record.PresentId);
  EXPECT_EQ(TimeSpan(Refresh), record.AnimationStep);
  EXPECT_EQ(TimeSpan(Refresh), record.DisplayStep);
  EXPECT_EQ(TimeSpan(0), record.Error);
  EXPECT_FALSE(animationError.TryPop(record));
}


// The example of the mb-framepacing README: a frame that is shown a refresh late moved too little, and the one after it is right
// again when the animation is not moved
TEST(TestSampleAnimationError, AFrameShownARefreshLate_NegativeError)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  animationError.AddFrame(1000, TimeSpan(100000000));
  animationError.AddFrame(1001, TimeSpan(100000000 + Refresh));
  animationError.AddFrame(1002, TimeSpan(100000000 + (2 * Refresh)));
  animationError.AddDisplayTime(1000, TickCount(0));
  animationError.AddDisplayTime(1001, TickCount(2 * Refresh));
  animationError.AddDisplayTime(1002, TickCount(3 * Refresh));

  ASSERT_TRUE(TryPopJudged(animationError, record));
  EXPECT_EQ(1001u, record.PresentId);
  EXPECT_EQ(TimeSpan(-Refresh), record.Error);
  ASSERT_TRUE(TryPopJudged(animationError, record));
  EXPECT_EQ(1002u, record.PresentId);
  EXPECT_EQ(TimeSpan(0), record.Error);
}


TEST(TestSampleAnimationError, AFrameShownSoonerThanItWasAnimatedFor_PositiveError)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  animationError.AddFrame(1, TimeSpan(0));
  animationError.AddFrame(2, TimeSpan(2 * Refresh));
  animationError.AddDisplayTime(1, TickCount(1000));
  animationError.AddDisplayTime(2, TickCount(1000 + Refresh));

  ASSERT_TRUE(TryPopJudged(animationError, record));
  EXPECT_EQ(TimeSpan(Refresh), record.Error);
}


TEST(TestSampleAnimationError, AFrameThatWasNotShown_TheStepIntoItAndTheStepOutOfItAreNotJudged)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  animationError.AddFrame(1, TimeSpan(0));
  animationError.AddFrame(2, TimeSpan(Refresh));
  animationError.AddFrame(3, TimeSpan(2 * Refresh));
  animationError.AddFrame(4, TimeSpan(3 * Refresh));
  animationError.AddDisplayTime(1, TickCount(1000));
  animationError.AddNotShown(2);
  // Frame 3 was shown a refresh late, which is not judged as the frame before it has no display time
  animationError.AddDisplayTime(3, TickCount(1000 + (3 * Refresh)));
  animationError.AddDisplayTime(4, TickCount(1000 + (4 * Refresh)));

  // Every frame is given, in the order they were presented in
  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(1u, record.PresentId);
  EXPECT_FALSE(record.IsJudged);
  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(2u, record.PresentId);
  EXPECT_FALSE(record.IsJudged);
  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(3u, record.PresentId);
  EXPECT_FALSE(record.IsJudged);
  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(4u, record.PresentId);
  EXPECT_TRUE(record.IsJudged);
  EXPECT_EQ(TimeSpan(Refresh), record.AnimationStep);
  EXPECT_EQ(TimeSpan(Refresh), record.DisplayStep);
  EXPECT_EQ(TimeSpan(0), record.Error);
  EXPECT_FALSE(animationError.TryPop(record));
  EXPECT_EQ(1u, animationError.CalcStats().Frames);
}


TEST(TestSampleAnimationError, ADisplayTimeAfterNotShown_TheFrameWasShown)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  animationError.AddFrame(1, TimeSpan(0));
  animationError.AddFrame(2, TimeSpan(Refresh));
  animationError.AddDisplayTime(1, TickCount(1000));
  animationError.AddNotShown(2);
  animationError.AddDisplayTime(2, TickCount(1000 + Refresh));
  // And a not shown that comes after the display time changes nothing
  animationError.AddNotShown(2);

  ASSERT_TRUE(TryPopJudged(animationError, record));
  EXPECT_EQ(2u, record.PresentId);
  EXPECT_EQ(TimeSpan(0), record.Error);
}


TEST(TestSampleAnimationError, TheFramesComeInTheOrderTheyWerePresentedIn)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  animationError.AddFrame(1, TimeSpan(0));
  animationError.AddFrame(2, TimeSpan(Refresh));
  animationError.AddFrame(3, TimeSpan(2 * Refresh));
  animationError.AddDisplayTime(1, TickCount(1000));
  // The display time of the third frame comes before the one of the second
  animationError.AddDisplayTime(3, TickCount(1000 + (2 * Refresh)));

  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(1u, record.PresentId);
  EXPECT_FALSE(animationError.TryPop(record));

  animationError.AddDisplayTime(2, TickCount(1000 + Refresh));

  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(2u, record.PresentId);
  EXPECT_TRUE(record.IsJudged);
  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(3u, record.PresentId);
  EXPECT_TRUE(record.IsJudged);
}


TEST(TestSampleAnimationError, AFrameNothingComesForIsGivenUpOn)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  animationError.AddFrame(1, TimeSpan(0));
  animationError.AddDisplayTime(1, TickCount(1000));
  ASSERT_TRUE(animationError.TryPop(record));
  // Nothing comes for frame 2
  animationError.AddFrame(2, TimeSpan(Refresh));
  for (uint64_t i = 0; i < (SampleAnimationError::MaxWaitFrames - 1u); ++i)
  {
    const uint64_t presentId = 3u + i;
    animationError.AddFrame(presentId, TimeSpan(static_cast<int64_t>(presentId - 1u) * Refresh));
    animationError.AddDisplayTime(presentId, TickCount(1000 + (static_cast<int64_t>(presentId - 1u) * Refresh)));
  }
  // The newest frame is MaxWaitFrames - 1 after frame 2: it is still waited for
  EXPECT_FALSE(animationError.TryPop(record));

  const uint64_t lastId = 2u + SampleAnimationError::MaxWaitFrames;
  animationError.AddFrame(lastId, TimeSpan(static_cast<int64_t>(lastId - 1u) * Refresh));

  // Frame 2 is given up on. Frame 3 comes after a frame that is not known, so the first error is the one of frame 4
  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(2u, record.PresentId);
  EXPECT_FALSE(record.IsJudged);
  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(3u, record.PresentId);
  EXPECT_FALSE(record.IsJudged);
  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(4u, record.PresentId);
  EXPECT_TRUE(record.IsJudged);
  EXPECT_EQ(TimeSpan(0), record.Error);
}


TEST(TestSampleAnimationError, AFullHolder_TheOldestFrameMakesRoomAndTheNextIsNotJudged)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  for (uint64_t presentId = 1; presentId <= (SampleAnimationError::Capacity + 2u); ++presentId)
  {
    animationError.AddFrame(presentId, TimeSpan(static_cast<int64_t>(presentId) * Refresh));
  }
  // Frame 1 and 2 are gone, a display time for them is ignored
  animationError.AddDisplayTime(1, TickCount(1000));
  animationError.AddDisplayTime(3, TickCount(3000));
  animationError.AddDisplayTime(4, TickCount(3000 + Refresh));

  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(3u, record.PresentId);
  EXPECT_FALSE(record.IsJudged);
  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_EQ(4u, record.PresentId);
  EXPECT_TRUE(record.IsJudged);
  EXPECT_EQ(TimeSpan(0), record.Error);
}


TEST(TestSampleAnimationError, Clear_ForgetsTheFrameThatWasShownLast)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  animationError.AddFrame(1, TimeSpan(0));
  animationError.AddFrame(2, TimeSpan(Refresh));
  animationError.AddDisplayTime(1, TickCount(1000));
  animationError.AddDisplayTime(2, TickCount(1000 + Refresh));
  ASSERT_TRUE(TryPopJudged(animationError, record));

  animationError.Clear();
  animationError.AddFrame(3, TimeSpan(100 * Refresh));
  animationError.AddDisplayTime(3, TickCount(1000 + (5 * Refresh)));

  ASSERT_TRUE(animationError.TryPop(record));
  EXPECT_FALSE(record.IsJudged);
  EXPECT_EQ(SampleAnimationErrorStats(), animationError.CalcStats());
}


TEST(TestSampleAnimationError, Stats_TheAverageWithoutSignAndTheWorstWithItsSign)
{
  SampleAnimationError animationError;
  SampleAnimationErrorRecord record;
  // Shown at 0, 2, 3 and 3.5 refreshes: the errors are -1, 0 and +0.5 refreshes
  animationError.AddFrame(1, TimeSpan(0));
  animationError.AddFrame(2, TimeSpan(Refresh));
  animationError.AddFrame(3, TimeSpan(2 * Refresh));
  animationError.AddFrame(4, TimeSpan(3 * Refresh));
  animationError.AddDisplayTime(1, TickCount(0));
  animationError.AddDisplayTime(2, TickCount(2 * Refresh));
  animationError.AddDisplayTime(3, TickCount(3 * Refresh));
  animationError.AddDisplayTime(4, TickCount((3 * Refresh) + (Refresh / 2)));
  while (animationError.TryPop(record))
  {
  }

  const SampleAnimationErrorStats stats = animationError.CalcStats();

  EXPECT_EQ(3u, stats.Frames);
  // -1 and +0.5 refreshes are beyond the threshold of 1 ms, 0 is not
  EXPECT_EQ(2u, stats.ErrorFrames);
  EXPECT_EQ(TimeSpan(-Refresh), stats.WorstError);
  // (166667 + 0 + 83334) / 3
  EXPECT_EQ(TimeSpan(83333), stats.AverageAbsError);
}


TEST(TestSampleAnimationError, ToRefreshThousandths)
{
  const TimeSpan refresh(Refresh);

  EXPECT_EQ(0, SampleAnimationError::ToRefreshThousandths(TimeSpan(0), refresh));
  EXPECT_EQ(-1000, SampleAnimationError::ToRefreshThousandths(TimeSpan(-Refresh), refresh));
  EXPECT_EQ(1000, SampleAnimationError::ToRefreshThousandths(TimeSpan(Refresh), refresh));
  EXPECT_EQ(500, SampleAnimationError::ToRefreshThousandths(TimeSpan(Refresh / 2), refresh));
  EXPECT_EQ(-500, SampleAnimationError::ToRefreshThousandths(TimeSpan(-(Refresh / 2)), refresh));
  // 1 ms of a 16.6667 ms refresh
  EXPECT_EQ(60, SampleAnimationError::ToRefreshThousandths(TimeSpan::FromMilliseconds(1), refresh));
}


TEST(TestSampleAnimationError, ToRefreshThousandths_AHugeErrorIsKeptToTheLargestValue)
{
  const TimeSpan refresh(Refresh);

  EXPECT_EQ(-SampleAnimationError::MaxRefreshThousandths, SampleAnimationError::ToRefreshThousandths(TimeSpan::FromSeconds(-100), refresh));
  EXPECT_EQ(SampleAnimationError::MaxRefreshThousandths, SampleAnimationError::ToRefreshThousandths(TimeSpan::FromSeconds(100), refresh));
}


TEST(TestSampleAnimationError, ToRefreshThousandths_WithoutARefreshPeriodItIsZero)
{
  EXPECT_EQ(0, SampleAnimationError::ToRefreshThousandths(TimeSpan(-Refresh), TimeSpan(0)));
}
