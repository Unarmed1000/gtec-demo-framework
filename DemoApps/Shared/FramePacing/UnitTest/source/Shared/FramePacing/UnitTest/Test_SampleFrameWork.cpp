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
#include <Shared/FramePacing/SampleFrameWork.hpp>

using namespace Fsl;

namespace
{
  using TestSampleFrameWork = TestFixtureFslBase;

  //! A frame that starts at the given time and whose CPU work takes the given time
  void RunFrame(SampleFrameWork& rWork, const uint64_t frameId, const int64_t startTicks, const int64_t cpuTicks)
  {
    rWork.BeginFrame(frameId, TickCount(startTicks));
    rWork.EndCpuWork(TickCount(startTicks + cpuTicks));
  }
}


TEST(TestSampleFrameWork, Empty_NothingToTake)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;

  EXPECT_FALSE(work.TryPop(record));
}


TEST(TestSampleFrameWork, AFrameWaitsForItsGpuTime)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 1, 1000, 300);

  EXPECT_FALSE(work.TryPop(record));

  work.AddGpuTime(1, TimeSpan(500), {});
  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(300, record.CpuTime.Ticks());
  EXPECT_EQ(500, record.GpuTime.Ticks());
  EXPECT_FALSE(work.TryPop(record));
}


TEST(TestSampleFrameWork, AFrameIsNotTakenBeforeItsCpuWorkEnded)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  work.BeginFrame(1, TickCount(1000));
  work.AddGpuTime(1, TimeSpan(500), {});

  EXPECT_FALSE(work.TryPop(record));

  work.EndCpuWork(TickCount(1300));
  EXPECT_TRUE(work.TryPop(record));
}


TEST(TestSampleFrameWork, FrameTime_WithTheEndOfTheGpuWork_IsToTheLaterEnd)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 1, 1000, 300);
  // The GPU began 100 after the CPU was done and worked for 500
  work.AddGpuInterval(1, TickCount(1400), TickCount(1900));

  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(300, record.CpuTime.Ticks());
  EXPECT_EQ(500, record.GpuTime.Ticks());
  // Not the two added: the time the GPU waited is in it
  EXPECT_EQ(900, record.FrameTime.Ticks());
}


TEST(TestSampleFrameWork, FrameTime_TheGpuDoneBeforeTheCpu_IsTheCpuTime)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 1, 1000, 800);
  work.AddGpuInterval(1, TickCount(1100), TickCount(1300));

  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(800, record.CpuTime.Ticks());
  EXPECT_EQ(200, record.GpuTime.Ticks());
  EXPECT_EQ(800, record.FrameTime.Ticks());
}


TEST(TestSampleFrameWork, FrameTime_WithoutTheEndOfTheGpuWork_TheGpuStartsWhenTheCpuIsDone)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 1, 1000, 300);
  work.AddGpuTime(1, TimeSpan(500), {});

  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(800, record.FrameTime.Ticks());
}


TEST(TestSampleFrameWork, GpuTimeAndInterval_OfTheSameFrame_AreOneRecord)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 1, 1000, 300);
  // The duration as a timer gives it, and the interval on the clock of the CPU
  work.AddGpuTime(1, TimeSpan(490), {});
  work.AddGpuInterval(1, TickCount(1400), TickCount(1900));

  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(490, record.GpuTime.Ticks());
  EXPECT_EQ(900, record.FrameTime.Ticks());
  EXPECT_FALSE(work.TryPop(record));
}


TEST(TestSampleFrameWork, GpuTimeWithItsEnd_AsATimerOfOpenGLESGivesIt)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 1, 1000, 300);
  work.AddGpuTime(1, TimeSpan(500), TickCount(2100));

  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(500, record.GpuTime.Ticks());
  EXPECT_EQ(1100, record.FrameTime.Ticks());
}


TEST(TestSampleFrameWork, FramesComeInTheOrderTheyStarted)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 1, 1000, 100);
  RunFrame(work, 2, 2000, 200);
  work.AddGpuTime(1, TimeSpan(10), {});
  work.AddGpuTime(2, TimeSpan(20), {});

  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(100, record.CpuTime.Ticks());
  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(200, record.CpuTime.Ticks());
  EXPECT_FALSE(work.TryPop(record));
}


TEST(TestSampleFrameWork, AFrameThatWasNotMeasured_IsTakenWhenALaterOneGetsItsGpuTime)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 1, 1000, 100);
  RunFrame(work, 2, 2000, 200);
  EXPECT_FALSE(work.TryPop(record));

  work.AddGpuTime(2, TimeSpan(20), {});

  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(100, record.CpuTime.Ticks());
  EXPECT_EQ(0, record.GpuTime.Ticks());
  EXPECT_EQ(100, record.FrameTime.Ticks());
  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(20, record.GpuTime.Ticks());
}


TEST(TestSampleFrameWork, WithoutAnyGpuTime_AFrameIsTakenAfterAFewFrames)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 1, 1000, 100);
  for (uint64_t frameId = 2; frameId <= SampleFrameWork::MaxWaitFrames; ++frameId)
  {
    RunFrame(work, frameId, static_cast<int64_t>(frameId) * 1000, 100);
    EXPECT_FALSE(work.TryPop(record));
  }

  RunFrame(work, SampleFrameWork::MaxWaitFrames + 1u, 9000, 100);

  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(100, record.CpuTime.Ticks());
  EXPECT_EQ(0, record.GpuTime.Ticks());
  EXPECT_EQ(100, record.FrameTime.Ticks());
  // The next one has not waited long enough
  EXPECT_FALSE(work.TryPop(record));
}


TEST(TestSampleFrameWork, AGpuTimeOfAFrameThatIsNotHeld_ChangesNoFrame)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 5, 1000, 100);

  work.AddGpuTime(4, TimeSpan(999), {});

  EXPECT_FALSE(work.TryPop(record));
  work.AddGpuTime(5, TimeSpan(20), {});
  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(20, record.GpuTime.Ticks());
}


TEST(TestSampleFrameWork, MoreFramesThanCanBeHeld_TheOldestMakesRoom)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  const auto frames = static_cast<uint64_t>(SampleFrameWork::Capacity) + 2u;
  for (uint64_t frameId = 1; frameId <= frames; ++frameId)
  {
    RunFrame(work, frameId, static_cast<int64_t>(frameId) * 1000, static_cast<int64_t>(frameId));
  }

  // The first two are gone
  ASSERT_TRUE(work.TryPop(record));
  EXPECT_EQ(3, record.CpuTime.Ticks());
}


TEST(TestSampleFrameWork, Clear_ForgetsEveryFrame)
{
  SampleFrameWork work;
  SampleFrameWorkRecord record;
  RunFrame(work, 1, 1000, 100);
  work.AddGpuTime(1, TimeSpan(10), {});

  work.Clear();

  EXPECT_FALSE(work.TryPop(record));
}
