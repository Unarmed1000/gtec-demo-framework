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

#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDemoService/FramePacingMarker/Impl/FramePacingFrameRecordUtil.hpp>

using namespace Fsl;

namespace
{
  using Test_FramePacingFrameRecordUtil = TestFixtureFslBase;

  //! A record as the service fills it in for a frame without a schedule
  FramePacingFrameRecord CreateHostRecord() noexcept
  {
    FramePacingFrameRecord record;
    record.FrameIndex = 42;
    record.AnimationTicks = 1000;
    record.CpuStartTicks = 2000;
    record.RunId = 7;
    return record;
  }

  //! The marker that was drawn for the frame before the one of CreateHostRecord, with the same animation time
  FramePacingMarkerInfo CreatePreviousMarker() noexcept
  {
    FramePacingMarkerInfo marker;
    marker.FrameIndex = 41;
    marker.AnimationTime = TimeSpan(1000);
    marker.RunId = 7;
    return marker;
  }
}


TEST(Test_FramePacingFrameRecordUtil, ApplySchedule_AnimationTimeOnly)
{
  FramePacingFrameRecord record = CreateHostRecord();
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = TimeSpan(5000);

  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_EQ(5000, record.AnimationTicks);
  // The CPU start time of the host is kept
  EXPECT_EQ(2000, record.CpuStartTicks);
  EXPECT_EQ(0, record.IntendedDisplayTicks);
  EXPECT_EQ(0, record.TargetFrameTicks);
  EXPECT_EQ(0, record.PreferredFrameTicks);
  EXPECT_FALSE(record.Static);
  // The values that are not part of the schedule are untouched
  EXPECT_EQ(42u, record.FrameIndex);
  EXPECT_EQ(7u, record.RunId);
}


TEST(Test_FramePacingFrameRecordUtil, ApplySchedule_AllValues)
{
  FramePacingFrameRecord record = CreateHostRecord();
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = TimeSpan(5000);
  schedule.CpuStartTime = TickCount(2100);
  schedule.IntendedDisplayTime = TickCount(335433);
  schedule.TargetFrameTime = TimeSpan(333333);
  schedule.PreferredFrameTime = TimeSpan(166667);
  schedule.Static = true;

  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_EQ(5000, record.AnimationTicks);
  EXPECT_EQ(2100, record.CpuStartTicks);
  EXPECT_EQ(335433, record.IntendedDisplayTicks);
  EXPECT_EQ(333333, record.TargetFrameTicks);
  EXPECT_EQ(166667, record.PreferredFrameTicks);
  EXPECT_TRUE(record.Static);
}


TEST(Test_FramePacingFrameRecordUtil, ApplySchedule_ReplacesPreviousPacingValues)
{
  FramePacingFrameRecord record = CreateHostRecord();
  record.IntendedDisplayTicks = 111;
  record.TargetFrameTicks = 222;
  record.PreferredFrameTicks = 333;
  record.Static = true;
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = TimeSpan(5000);

  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_EQ(0, record.IntendedDisplayTicks);
  EXPECT_EQ(0, record.TargetFrameTicks);
  EXPECT_EQ(0, record.PreferredFrameTicks);
  EXPECT_FALSE(record.Static);
}


TEST(Test_FramePacingFrameRecordUtil, ApplySchedule_NotPositiveTimesAreUnknown)
{
  FramePacingFrameRecord record = CreateHostRecord();
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = TimeSpan(5000);
  schedule.CpuStartTime = TickCount(0);
  schedule.IntendedDisplayTime = TickCount(-1);
  schedule.TargetFrameTime = TimeSpan(0);
  schedule.PreferredFrameTime = TimeSpan(-166667);

  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_EQ(0, record.CpuStartTicks);
  EXPECT_EQ(0, record.IntendedDisplayTicks);
  EXPECT_EQ(0, record.TargetFrameTicks);
  EXPECT_EQ(0, record.PreferredFrameTicks);
}


TEST(Test_FramePacingFrameRecordUtil, ToKnownTicks)
{
  EXPECT_EQ(0, FramePacingFrameRecordUtil::ToKnownTicks(std::optional<TimeSpan>()));
  EXPECT_EQ(0, FramePacingFrameRecordUtil::ToKnownTicks(std::optional<TimeSpan>(TimeSpan(0))));
  EXPECT_EQ(0, FramePacingFrameRecordUtil::ToKnownTicks(std::optional<TimeSpan>(TimeSpan(-5))));
  EXPECT_EQ(5, FramePacingFrameRecordUtil::ToKnownTicks(std::optional<TimeSpan>(TimeSpan(5))));
  EXPECT_EQ(9, FramePacingFrameRecordUtil::ToKnownTicks(std::optional<TickCount>(TickCount(9))));
}


TEST(Test_FramePacingFrameRecordUtil, IsStaticBefore_SameAnimationTime)
{
  const FramePacingFrameRecord record = CreateHostRecord();

  EXPECT_TRUE(FramePacingFrameRecordUtil::IsStaticBefore(record, CreatePreviousMarker()));
}


TEST(Test_FramePacingFrameRecordUtil, IsStaticBefore_AnimationTimeMoved)
{
  const FramePacingFrameRecord record = CreateHostRecord();
  FramePacingMarkerInfo marker = CreatePreviousMarker();
  marker.AnimationTime = TimeSpan(999);

  EXPECT_FALSE(FramePacingFrameRecordUtil::IsStaticBefore(record, marker));
}


TEST(Test_FramePacingFrameRecordUtil, IsStaticBefore_ScheduleDecidesTheAnimationTime)
{
  // The host's animation time stands still, but the app animates the frame by its own time
  FramePacingFrameRecord record = CreateHostRecord();
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = TimeSpan(5000);
  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_FALSE(FramePacingFrameRecordUtil::IsStaticBefore(record, CreatePreviousMarker()));

  // And the other way around: the app's animation time stands still
  FramePacingMarkerInfo marker = CreatePreviousMarker();
  marker.AnimationTime = TimeSpan(5000);
  EXPECT_TRUE(FramePacingFrameRecordUtil::IsStaticBefore(record, marker));
}


TEST(Test_FramePacingFrameRecordUtil, IsStaticBefore_NotTheFrameRightBefore)
{
  const FramePacingFrameRecord record = CreateHostRecord();
  FramePacingMarkerInfo marker = CreatePreviousMarker();

  // A frame was started in between that never drew a marker
  marker.FrameIndex = 40;
  EXPECT_FALSE(FramePacingFrameRecordUtil::IsStaticBefore(record, marker));
  // The marker of the frame itself (it is asked again after its marker was drawn)
  marker.FrameIndex = 42;
  EXPECT_FALSE(FramePacingFrameRecordUtil::IsStaticBefore(record, marker));
}


TEST(Test_FramePacingFrameRecordUtil, IsStaticBefore_AnotherRun)
{
  const FramePacingFrameRecord record = CreateHostRecord();
  FramePacingMarkerInfo marker = CreatePreviousMarker();
  marker.RunId = 6;

  EXPECT_FALSE(FramePacingFrameRecordUtil::IsStaticBefore(record, marker));
}
