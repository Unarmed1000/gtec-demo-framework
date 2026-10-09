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
    record.AnimationNanoseconds = 1000;
    record.CpuStartNanoseconds = 2000;
    record.RunId = 7;
    return record;
  }

  //! The marker that was drawn for the frame before the one of CreateHostRecord, with the same animation time
  FramePacingMarkerInfo CreatePreviousMarker() noexcept
  {
    FramePacingMarkerInfo marker;
    marker.FrameIndex = 41;
    marker.AnimationTime = NanosecondTimeSpan(1000);
    marker.RunId = 7;
    return marker;
  }
}


TEST(Test_FramePacingFrameRecordUtil, ApplySchedule_AnimationTimeOnly)
{
  FramePacingFrameRecord record = CreateHostRecord();
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = NanosecondTimeSpan(5000);

  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_EQ(5000, record.AnimationNanoseconds);
  // The CPU start time of the host is kept
  EXPECT_EQ(2000, record.CpuStartNanoseconds);
  EXPECT_EQ(0, record.IntendedDisplayNanoseconds);
  EXPECT_EQ(0, record.TargetFrameNanoseconds);
  EXPECT_EQ(0, record.PreferredFrameNanoseconds);
  EXPECT_FALSE(record.Static);
  // The values that are not part of the schedule are untouched
  EXPECT_EQ(42u, record.FrameIndex);
  EXPECT_EQ(7u, record.RunId);
}


TEST(Test_FramePacingFrameRecordUtil, ApplySchedule_AllValues)
{
  FramePacingFrameRecord record = CreateHostRecord();
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = NanosecondTimeSpan(5000);
  schedule.CpuStartTime = NanosecondTickCount(2100);
  schedule.CpuBusyTime = NanosecondTimeSpan(41000);
  schedule.IntendedDisplayTime = NanosecondTickCount(335433);
  schedule.TargetFrameTime = NanosecondTimeSpan(33333333);
  schedule.PreferredFrameTime = NanosecondTimeSpan(16666667);
  schedule.Static = true;

  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_EQ(5000, record.AnimationNanoseconds);
  EXPECT_EQ(2100, record.CpuStartNanoseconds);
  EXPECT_EQ(41000, record.CpuBusyNanoseconds);
  EXPECT_EQ(335433, record.IntendedDisplayNanoseconds);
  EXPECT_EQ(33333333, record.TargetFrameNanoseconds);
  EXPECT_EQ(16666667, record.PreferredFrameNanoseconds);
  EXPECT_TRUE(record.Static);
}


TEST(Test_FramePacingFrameRecordUtil, ApplySchedule_ReplacesPreviousPacingValues)
{
  FramePacingFrameRecord record = CreateHostRecord();
  record.IntendedDisplayNanoseconds = 111;
  record.TargetFrameNanoseconds = 222;
  record.PreferredFrameNanoseconds = 333;
  record.Static = true;
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = NanosecondTimeSpan(5000);

  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_EQ(0, record.IntendedDisplayNanoseconds);
  EXPECT_EQ(0, record.TargetFrameNanoseconds);
  EXPECT_EQ(0, record.PreferredFrameNanoseconds);
  EXPECT_FALSE(record.Static);
}


TEST(Test_FramePacingFrameRecordUtil, ApplySchedule_NotPositiveTimesAreUnknown)
{
  FramePacingFrameRecord record = CreateHostRecord();
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = NanosecondTimeSpan(5000);
  schedule.CpuStartTime = NanosecondTickCount(0);
  schedule.IntendedDisplayTime = NanosecondTickCount(-1);
  schedule.TargetFrameTime = NanosecondTimeSpan(0);
  schedule.PreferredFrameTime = NanosecondTimeSpan(-16666667);

  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_EQ(0, record.CpuStartNanoseconds);
  EXPECT_EQ(0, record.IntendedDisplayNanoseconds);
  EXPECT_EQ(0, record.TargetFrameNanoseconds);
  EXPECT_EQ(0, record.PreferredFrameNanoseconds);
}


TEST(Test_FramePacingFrameRecordUtil, ApplySchedule_ANanosecondIsKept)
{
  // The refresh period of a 240.016 Hz mode is not a whole number of ticks of 100ns: the marker gets what the app gave
  FramePacingFrameRecord record = CreateHostRecord();
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = NanosecondTimeSpan(770783689);
  schedule.CpuStartTime = NanosecondTickCount(390293215838301);
  schedule.CpuBusyTime = NanosecondTimeSpan(125199);
  schedule.IntendedDisplayTime = NanosecondTickCount(390293217625389);
  schedule.TargetFrameTime = NanosecondTimeSpan(4166389);
  schedule.PreferredFrameTime = NanosecondTimeSpan(4166389);

  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_EQ(770783689, record.AnimationNanoseconds);
  EXPECT_EQ(390293215838301, record.CpuStartNanoseconds);
  EXPECT_EQ(125199, record.CpuBusyNanoseconds);
  EXPECT_EQ(390293217625389, record.IntendedDisplayNanoseconds);
  EXPECT_EQ(4166389, record.TargetFrameNanoseconds);
  EXPECT_EQ(4166389, record.PreferredFrameNanoseconds);
}


TEST(Test_FramePacingFrameRecordUtil, ToKnownNanoseconds)
{
  EXPECT_EQ(0, FramePacingFrameRecordUtil::ToKnownNanoseconds(std::optional<NanosecondTimeSpan>()));
  EXPECT_EQ(0, FramePacingFrameRecordUtil::ToKnownNanoseconds(std::optional<NanosecondTimeSpan>(NanosecondTimeSpan(0))));
  EXPECT_EQ(0, FramePacingFrameRecordUtil::ToKnownNanoseconds(std::optional<NanosecondTimeSpan>(NanosecondTimeSpan(-5))));
  EXPECT_EQ(5, FramePacingFrameRecordUtil::ToKnownNanoseconds(std::optional<NanosecondTimeSpan>(NanosecondTimeSpan(5))));
  EXPECT_EQ(9, FramePacingFrameRecordUtil::ToKnownNanoseconds(std::optional<NanosecondTickCount>(NanosecondTickCount(9))));
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
  marker.AnimationTime = NanosecondTimeSpan(999);

  EXPECT_FALSE(FramePacingFrameRecordUtil::IsStaticBefore(record, marker));
}


TEST(Test_FramePacingFrameRecordUtil, IsStaticBefore_ScheduleDecidesTheAnimationTime)
{
  // The host's animation time stands still, but the app animates the frame by its own time
  FramePacingFrameRecord record = CreateHostRecord();
  FramePacingFrameSchedule schedule;
  schedule.AnimationTime = NanosecondTimeSpan(5000);
  FramePacingFrameRecordUtil::ApplySchedule(record, schedule);

  EXPECT_FALSE(FramePacingFrameRecordUtil::IsStaticBefore(record, CreatePreviousMarker()));

  // And the other way around: the app's animation time stands still
  FramePacingMarkerInfo marker = CreatePreviousMarker();
  marker.AnimationTime = NanosecondTimeSpan(5000);
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
