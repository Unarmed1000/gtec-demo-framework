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
#include <Shared/FramePacing/SampleStatsLevelUtil.hpp>

using namespace Fsl;

namespace
{
  using TestSampleStatsLevelUtil = TestFixtureFslBase;

  // A frame time of 4 ms in ticks: a number the percentages of the tests divide without a rest
  constexpr TimeSpan Target(40000);
}


TEST(TestSampleStatsLevelUtil, Max)
{
  EXPECT_EQ(SampleStatsLevel::Normal, SampleStatsLevelUtil::Max(SampleStatsLevel::Normal, SampleStatsLevel::Normal));
  EXPECT_EQ(SampleStatsLevel::Warning, SampleStatsLevelUtil::Max(SampleStatsLevel::Normal, SampleStatsLevel::Warning));
  EXPECT_EQ(SampleStatsLevel::Warning, SampleStatsLevelUtil::Max(SampleStatsLevel::Warning, SampleStatsLevel::Normal));
  EXPECT_EQ(SampleStatsLevel::Error, SampleStatsLevelUtil::Max(SampleStatsLevel::Warning, SampleStatsLevel::Error));
  EXPECT_EQ(SampleStatsLevel::Error, SampleStatsLevelUtil::Max(SampleStatsLevel::Error, SampleStatsLevel::Normal));
}


TEST(TestSampleStatsLevelUtil, RateLateFrames)
{
  EXPECT_EQ(SampleStatsLevel::Normal, SampleStatsLevelUtil::RateLateFrames(0, 0));
  EXPECT_EQ(SampleStatsLevel::Normal, SampleStatsLevelUtil::RateLateFrames(0, 240));
  // Any late frame is worth a look
  EXPECT_EQ(SampleStatsLevel::Warning, SampleStatsLevelUtil::RateLateFrames(1, 240));
  EXPECT_EQ(SampleStatsLevel::Warning, SampleStatsLevelUtil::RateLateFrames(11, 240));
  // One in twenty is a error
  EXPECT_EQ(SampleStatsLevel::Error, SampleStatsLevelUtil::RateLateFrames(12, 240));
  EXPECT_EQ(SampleStatsLevel::Error, SampleStatsLevelUtil::RateLateFrames(240, 240));
}


TEST(TestSampleStatsLevelUtil, RateFrameTime)
{
  EXPECT_EQ(SampleStatsLevel::Normal, SampleStatsLevelUtil::RateFrameTime(Target, Target, Target));
  // No target: nothing to compare with
  EXPECT_EQ(SampleStatsLevel::Normal, SampleStatsLevelUtil::RateFrameTime(Target, TimeSpan(Target.Ticks() * 10), TimeSpan()));
  // The longest frame missed a refresh
  EXPECT_EQ(SampleStatsLevel::Normal, SampleStatsLevelUtil::RateFrameTime(Target, TimeSpan((Target.Ticks() * 149) / 100), Target));
  EXPECT_EQ(SampleStatsLevel::Warning, SampleStatsLevelUtil::RateFrameTime(Target, TimeSpan((Target.Ticks() * 150) / 100), Target));
  EXPECT_EQ(SampleStatsLevel::Warning, SampleStatsLevelUtil::RateFrameTime(Target, TimeSpan(Target.Ticks() * 5), Target));
  // The average is too long
  EXPECT_EQ(SampleStatsLevel::Warning,
            SampleStatsLevelUtil::RateFrameTime(TimeSpan((Target.Ticks() * 104) / 100), TimeSpan(Target.Ticks() * 2), Target));
  EXPECT_EQ(SampleStatsLevel::Error, SampleStatsLevelUtil::RateFrameTime(TimeSpan((Target.Ticks() * 106) / 100), Target, Target));
  EXPECT_EQ(SampleStatsLevel::Error, SampleStatsLevelUtil::RateFrameTime(TimeSpan(Target.Ticks() * 2), TimeSpan(Target.Ticks() * 2), Target));
}


TEST(TestSampleStatsLevelUtil, RateWork)
{
  EXPECT_EQ(SampleStatsLevel::Normal, SampleStatsLevelUtil::RateWork(TimeSpan(), Target));
  EXPECT_EQ(SampleStatsLevel::Normal, SampleStatsLevelUtil::RateWork(Target, TimeSpan()));
  EXPECT_EQ(SampleStatsLevel::Normal, SampleStatsLevelUtil::RateWork(TimeSpan((Target.Ticks() * 79) / 100), Target));
  EXPECT_EQ(SampleStatsLevel::Warning, SampleStatsLevelUtil::RateWork(TimeSpan((Target.Ticks() * 81) / 100), Target));
  EXPECT_EQ(SampleStatsLevel::Warning, SampleStatsLevelUtil::RateWork(TimeSpan((Target.Ticks() * 99) / 100), Target));
  EXPECT_EQ(SampleStatsLevel::Error, SampleStatsLevelUtil::RateWork(Target, Target));
  EXPECT_EQ(SampleStatsLevel::Error, SampleStatsLevelUtil::RateWork(TimeSpan(Target.Ticks() * 3), Target));
}
