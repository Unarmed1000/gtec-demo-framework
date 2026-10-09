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
#include <Shared/FramePacing/SamplePacer.hpp>
#include <Shared/FramePacing/SamplePacerKind.hpp>
#include <Shared/FramePacing/SamplePacerPlan.hpp>

using namespace Fsl;

namespace
{
  using TestSamplePacer = TestFixtureFslBase;

  //! A display of 100 Hz: a refresh period of 10 ms, which is a whole number of the ticks of the framework
  constexpr NanosecondTimeSpan RefreshPeriod(NanosecondTimeSpan::NanosecondsPerMillisecond * 10);
  constexpr int64_t PeriodTicks = TimeSpan::TicksPerMillisecond * 10;
  //! When the first frame of a test starts
  constexpr int64_t StartTicks = TimeSpan::TicksPerSecond * 100;
  //! How far into a refresh period the pacers present a frame they hold on a timer: a millisecond at this refresh rate
  constexpr int64_t MarginTicks = TimeSpan::TicksPerMillisecond;
  //! The default of the library for how long a wait for a present may take: four swap intervals of the frame and 50 ms at the
  //! least, which is the longer one at the refresh period of the tests
  constexpr int64_t PresentWaitTimeoutTicks = TimeSpan::TicksPerMillisecond * 50;

  constexpr TickCount At(const int64_t ticksAfterStart) noexcept
  {
    return TickCount(StartTicks + ticksAfterStart);
  }

  //! One refresh per frame, never another swap interval. The aim is low latency unless a test says otherwise: a pacer then keeps
  //! no frames in reserve, so a frame starts when the test says. The app of the tests has the vertical blank times and can wait
  //! for a present, so every kind paces as itself.
  SamplePacerConfig Config(const SamplePacerKind kind, const uint32_t waitingPresents = 2, const SamplePacerAim aim = SamplePacerAim::LowLatency)
  {
    SamplePacerConfig config;
    config.RefreshPeriod = RefreshPeriod;
    config.Adaptive = false;
    config.Kind = kind;
    config.Aim = aim;
    config.WaitingPresents = waitingPresents;
    config.Capabilities.VBlankTimes = true;
    config.Capabilities.WaitForPresent = true;
    return config;
  }

  //! A frame that starts at the given time, works for a tenth of a refresh and is presented right away
  uint64_t RunFrame(SamplePacer& rPacer, const int64_t startTicksAfterStart, const bool accepted = true)
  {
    const SamplePacerSchedule schedule = rPacer.BeginFrame(At(startTicksAfterStart));
    const SamplePacerPresentPlan presentPlan = rPacer.EndFrame(At(startTicksAfterStart + (PeriodTicks / 10)));
    SamplePacerPresentReport report;
    report.FrameId = presentPlan.FrameId;
    report.CallTime = At(startTicksAfterStart + (PeriodTicks / 10));
    report.ReturnTime = report.CallTime;
    report.Accepted = accepted;
    rPacer.AddPresent(report);
    return schedule.FrameId;
  }
}


TEST(TestSamplePacer, TimerPeriodOnly_FirstFrameStartsAtOnce)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  const SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));

  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, pacer.GetTier());
  const SamplePacerFrameStartPlan startPlan = pacer.PlanFrame(At(0));
  EXPECT_EQ(0u, startPlan.WaitForPresentFrameId);
  EXPECT_EQ(0, startPlan.StartTime.Ticks());
}


TEST(TestSamplePacer, TimerPeriodOnly_NextFrameIsDueARefreshLater)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));

  const SamplePacerSchedule schedule = pacer.BeginFrame(At(0));
  EXPECT_EQ(1u, schedule.FrameId);
  EXPECT_EQ(1u, schedule.SwapInterval);
  EXPECT_EQ(At(PeriodTicks), schedule.NextFrameStartTime);

  // A frame of one refresh is presented at once
  const SamplePacerPresentPlan presentPlan = pacer.EndFrame(At(PeriodTicks / 5));
  EXPECT_EQ(1u, presentPlan.FrameId);
  EXPECT_EQ(0, presentPlan.PresentTime.Ticks());
  EXPECT_EQ(1u, presentPlan.SwapInterval);
  EXPECT_EQ(PeriodTicks / 5, presentPlan.CpuBusy.Ticks());

  const SamplePacerFrameStartPlan startPlan = pacer.PlanFrame(At(PeriodTicks / 4));
  EXPECT_EQ(0u, startPlan.WaitForPresentFrameId);
  EXPECT_EQ(At(PeriodTicks), startPlan.StartTime);
}


TEST(TestSamplePacer, TimerPeriodOnly_PlanFrameChangesNothing)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));
  RunFrame(pacer, 0);

  const SamplePacerFrameStartPlan first = pacer.PlanFrame(At(PeriodTicks / 4));
  const SamplePacerFrameStartPlan second = pacer.PlanFrame(At(PeriodTicks / 4));

  EXPECT_EQ(first.StartTime, second.StartTime);
  EXPECT_EQ(At(PeriodTicks), second.StartTime);
}


TEST(TestSamplePacer, TimerPeriodOnly_AFrameLessThanHalfAPeriodLateKeepsItsStep)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));
  RunFrame(pacer, 0);

  // Its step has passed, so it starts at once, and the frame after it is due at the step after that
  const int64_t lateStartTicks = PeriodTicks + ((PeriodTicks * 3) / 10);
  EXPECT_EQ(0, pacer.PlanFrame(At(lateStartTicks)).StartTime.Ticks());
  const SamplePacerSchedule schedule = pacer.BeginFrame(At(lateStartTicks));

  EXPECT_EQ(At(PeriodTicks * 2), schedule.NextFrameStartTime);
  EXPECT_EQ(0u, pacer.GetRefreshesBehindClock());
}


TEST(TestSamplePacer, TimerPeriodOnly_AfterALongFrameTheNextWaitsForTheNearestStep)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));
  RunFrame(pacer, 0);

  // The loop comes back 2.7 refreshes after the first frame started: the step nearest to that is the third
  const int64_t backTicks = (PeriodTicks * 27) / 10;
  const SamplePacerFrameStartPlan startPlan = pacer.PlanFrame(At(backTicks));
  EXPECT_EQ(At(PeriodTicks * 3), startPlan.StartTime);

  const SamplePacerSchedule schedule = pacer.BeginFrame(At(PeriodTicks * 3));
  EXPECT_EQ(At(PeriodTicks * 4), schedule.NextFrameStartTime);
  // The animation moves on by the swap interval of the frame, the two refreshes that were lost are counted
  EXPECT_EQ(PeriodTicks * NanosecondTimeSpan::NanosecondsPerTick, schedule.AnimationStep.TotalNanoseconds());
  EXPECT_EQ(2u, pacer.GetRefreshesBehindClock());
}


TEST(TestSamplePacer, TimerPeriodOnly_AFrameOfFourRefreshesIsPresentedOnATimer)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.TargetFps = 25;
  SamplePacer pacer(config);

  const SamplePacerSchedule schedule = pacer.BeginFrame(At(0));
  EXPECT_EQ(4u, schedule.SwapInterval);
  EXPECT_EQ(At(PeriodTicks * 4), schedule.NextFrameStartTime);

  // The present holds the frame for one refresh, so it is made in the refresh before the step the next frame is due at
  const SamplePacerPresentPlan presentPlan = pacer.EndFrame(At(PeriodTicks / 5));
  EXPECT_EQ(1u, presentPlan.SwapInterval);
  EXPECT_EQ(At((PeriodTicks * 3) + MarginTicks), presentPlan.PresentTime);

  // And the next frame starts at that step
  EXPECT_EQ(At(PeriodTicks * 4), pacer.PlanFrame(At((PeriodTicks * 3) + (MarginTicks * 2))).StartTime);
}


TEST(TestSamplePacer, TimerPeriodOnly_NeverAsksForAPresentWait)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly, 1));
  RunFrame(pacer, 0);
  RunFrame(pacer, PeriodTicks);

  EXPECT_EQ(0u, pacer.PlanFrame(At(PeriodTicks + (PeriodTicks / 4))).WaitForPresentFrameId);
}


TEST(TestSamplePacer, TimerWaitForPresent_NothingToWaitForBeforeAPresent)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerWaitForPresent, 1));

  EXPECT_EQ(0u, pacer.PlanFrame(At(0)).WaitForPresentFrameId);
  pacer.BeginFrame(At(0));
  pacer.EndFrame(At(PeriodTicks / 10));
  // The frame was not presented yet
  EXPECT_EQ(0u, pacer.PlanFrame(At(PeriodTicks / 5)).WaitForPresentFrameId);
}


TEST(TestSamplePacer, TimerWaitForPresent_WaitingPresents1_WaitsForTheLastPresent)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerWaitForPresent, 1));
  const uint64_t frameId = RunFrame(pacer, 0);

  const SamplePacerFrameStartPlan startPlan = pacer.PlanFrame(At(PeriodTicks / 4));

  EXPECT_EQ(frameId, startPlan.WaitForPresentFrameId);
  EXPECT_EQ(PresentWaitTimeoutTicks, startPlan.WaitForPresentTimeout.Ticks());
  // The time comes after the present
  EXPECT_EQ(At(PeriodTicks), startPlan.StartTime);
}


TEST(TestSamplePacer, TimerWaitForPresent_WaitingPresents2_WaitsForThePresentBeforeTheLast)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerWaitForPresent, 2));
  const uint64_t firstFrameId = RunFrame(pacer, 0);
  // One present may wait
  EXPECT_EQ(0u, pacer.PlanFrame(At(PeriodTicks / 4)).WaitForPresentFrameId);

  RunFrame(pacer, PeriodTicks);

  EXPECT_EQ(firstFrameId, pacer.PlanFrame(At(PeriodTicks + (PeriodTicks / 4))).WaitForPresentFrameId);
}


TEST(TestSamplePacer, TimerWaitForPresent_APresentThatWasNotTakenIsNotWaitedFor)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerWaitForPresent, 1));
  RunFrame(pacer, 0);
  RunFrame(pacer, PeriodTicks, false);

  // Not the one that was not taken, and not the one before it
  EXPECT_EQ(0u, pacer.PlanFrame(At(PeriodTicks + (PeriodTicks / 4))).WaitForPresentFrameId);

  // The next one that is taken is waited for again
  const uint64_t frameId = RunFrame(pacer, PeriodTicks * 2);
  EXPECT_EQ(frameId, pacer.PlanFrame(At((PeriodTicks * 2) + (PeriodTicks / 4))).WaitForPresentFrameId);
}


TEST(TestSamplePacer, TimerWaitForPresent_AFrameThatIsPresentedAgainCanBeWaitedFor)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerWaitForPresent, 1));
  RunFrame(pacer, 0);
  // Its present was not taken, and the frame is presented again (on the swapchain that was made anew)
  const uint64_t frameId = RunFrame(pacer, PeriodTicks, false);
  SamplePacerPresentReport report;
  report.FrameId = frameId;
  report.CallTime = At(PeriodTicks + (PeriodTicks / 5));
  report.ReturnTime = report.CallTime;
  report.Accepted = true;
  pacer.AddPresent(report);

  EXPECT_EQ(frameId, pacer.PlanFrame(At(PeriodTicks + (PeriodTicks / 4))).WaitForPresentFrameId);
}


TEST(TestSamplePacer, TimerWaitForPresent_AfterForgetPresentsNoPresentFromBeforeIsWaitedFor)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerWaitForPresent, 1));
  RunFrame(pacer, 0);
  ASSERT_NE(0u, pacer.PlanFrame(At(PeriodTicks / 4)).WaitForPresentFrameId);

  pacer.ForgetPresents();

  const SamplePacerFrameStartPlan startPlan = pacer.PlanFrame(At(PeriodTicks / 4));
  EXPECT_EQ(0u, startPlan.WaitForPresentFrameId);
  // Nothing else changed: the frame is still due a refresh after the one before
  EXPECT_EQ(At(PeriodTicks), startPlan.StartTime);

  // And the next present is waited for again
  const uint64_t frameId = RunFrame(pacer, PeriodTicks);
  EXPECT_EQ(frameId, pacer.PlanFrame(At(PeriodTicks + (PeriodTicks / 4))).WaitForPresentFrameId);
}


TEST(TestSamplePacer, TimerWaitForPresent_AfterTheWaitThePresentIsNotAskedForAgain)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerWaitForPresent, 1));
  const uint64_t frameId = RunFrame(pacer, 0);
  ASSERT_EQ(frameId, pacer.PlanFrame(At(PeriodTicks / 4)).WaitForPresentFrameId);

  SamplePacerPresentWaitReport report;
  report.FrameId = frameId;
  report.BeginTime = At(PeriodTicks / 4);
  report.EndTime = At((PeriodTicks / 4) + (PeriodTicks / 100));
  report.Shown = true;
  pacer.AddPresentWait(report);

  // The frame is planned again: only the time is left
  const SamplePacerFrameStartPlan startPlan = pacer.PlanFrame(report.EndTime);
  EXPECT_EQ(0u, startPlan.WaitForPresentFrameId);
  EXPECT_EQ(At(PeriodTicks), startPlan.StartTime);
}


TEST(TestSamplePacer, TimerPeriodOnly_CpuBusyAtIsCountedFromTheStartOfTheFrame)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));
  // No frame was begun
  EXPECT_EQ(0, pacer.GetCpuBusyAt(At(0)).Ticks());

  pacer.BeginFrame(At(0));

  EXPECT_EQ(PeriodTicks / 8, pacer.GetCpuBusyAt(At(PeriodTicks / 8)).Ticks());
}


TEST(TestSamplePacer, TimerPeriodOnly_TheGpuTimeIsTheOneOfTheNewestReport)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));
  EXPECT_EQ(0, pacer.GetGpuTime().Ticks());
  const uint64_t frameId = RunFrame(pacer, 0);

  // When the GPU worked on the frame
  SamplePacerGpuWorkReport report;
  report.FrameId = frameId;
  report.BeginTime = At(PeriodTicks / 10);
  report.EndTime = At((PeriodTicks / 10) + (PeriodTicks / 4));
  report.Duration = TimeSpan(PeriodTicks / 4);
  pacer.AddGpuWork(report);
  EXPECT_EQ(PeriodTicks / 4, pacer.GetGpuTime().Ticks());

  // Only how long it worked on the next one
  const uint64_t nextFrameId = RunFrame(pacer, PeriodTicks);
  SamplePacerGpuWorkReport durationReport;
  durationReport.FrameId = nextFrameId;
  durationReport.Duration = TimeSpan(PeriodTicks / 2);
  pacer.AddGpuWork(durationReport);
  EXPECT_EQ(PeriodTicks / 2, pacer.GetGpuTime().Ticks());
}


TEST(TestSamplePacer, TimerPeriodOnly_ALaterGpuReportOfTheSameFrameTakesThePlaceOfTheFirst)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));
  const uint64_t frameId = RunFrame(pacer, 0);

  // How long the GPU worked is known first (a GPU timer)
  SamplePacerGpuWorkReport durationReport;
  durationReport.FrameId = frameId;
  durationReport.Duration = TimeSpan(PeriodTicks / 2);
  pacer.AddGpuWork(durationReport);
  EXPECT_EQ(PeriodTicks / 2, pacer.GetGpuTime().Ticks());

  // and when it worked afterwards (the same result placed on the clock of the app): the sample gives both, without holding the first
  SamplePacerGpuWorkReport timesReport;
  timesReport.FrameId = frameId;
  timesReport.BeginTime = At(PeriodTicks / 10);
  timesReport.EndTime = At((PeriodTicks / 10) + (PeriodTicks / 4));
  timesReport.Duration = TimeSpan(PeriodTicks / 4);
  pacer.AddGpuWork(timesReport);
  EXPECT_EQ(PeriodTicks / 4, pacer.GetGpuTime().Ticks());
}


TEST(TestSamplePacer, TimerPeriodOnly_MakesOnePauseAfterItStarted)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));
  EXPECT_EQ(0u, pacer.GetStartupPauses());

  // A second of frames, each started when the pacer says and presented right away
  int64_t startTicks = 0;
  for (int32_t i = 0; i < 100; ++i)
  {
    const SamplePacerFrameStartPlan startPlan = pacer.PlanFrame(At(startTicks + (PeriodTicks / 5)));
    if (startPlan.StartTime.Ticks() != 0)
    {
      startTicks = startPlan.StartTime.Ticks() - At(0).Ticks();
    }
    RunFrame(pacer, startTicks);
  }

  EXPECT_EQ(1u, pacer.GetStartupPauses());
}


TEST(TestSamplePacer, TimerPeriodOnly_Smoothness_MakesNoPauseAndStartsItsFirstFramesBackToBack)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  // The default of a config is the aim the library has as its default
  EXPECT_EQ(SamplePacerAim::Smoothness, SamplePacerConfig().Aim);

  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly, 2, SamplePacerAim::Smoothness));

  // The first frame of a start is given no time: the frames that are the reserve are made back to back
  EXPECT_EQ(0, pacer.PlanFrame(At(0)).StartTime.Ticks());

  // A second of frames, each started when the pacer says and presented right away
  int64_t startTicks = 0;
  uint32_t timedStarts = 0;
  for (int32_t i = 0; i < 100; ++i)
  {
    const SamplePacerFrameStartPlan startPlan = pacer.PlanFrame(At(startTicks + (PeriodTicks / 5)));
    if (startPlan.StartTime.Ticks() != 0)
    {
      startTicks = startPlan.StartTime.Ticks() - At(0).Ticks();
      ++timedStarts;
    }
    RunFrame(pacer, startTicks);
  }

  // The frames are paced, and the pause after a start belongs to the low latency aim
  EXPECT_GT(timedStarts, 90u);
  EXPECT_EQ(0u, pacer.GetStartupPauses());
}


TEST(TestSamplePacer, TimerPeriodOnly_NoPauseWhenItIsSetToNone)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.StartupPauseRefreshes = 0;
  SamplePacer pacer(config);

  for (int32_t i = 0; i < 100; ++i)
  {
    RunFrame(pacer, PeriodTicks * i);
  }

  EXPECT_EQ(0u, pacer.GetStartupPauses());
}


TEST(TestSamplePacer, TimerWaitForPresent_AWaitThatEndedWithoutThePresentIsCounted)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerWaitForPresent, 1));
  const uint64_t frameId = RunFrame(pacer, 0);
  EXPECT_EQ(0u, pacer.GetPresentWaitTimeouts());

  SamplePacerPresentWaitReport report;
  report.FrameId = frameId;
  report.BeginTime = At(PeriodTicks / 4);
  report.EndTime = At((PeriodTicks / 4) + PresentWaitTimeoutTicks);
  report.Shown = false;
  pacer.AddPresentWait(report);
  EXPECT_EQ(1u, pacer.GetPresentWaitTimeouts());

  report.Shown = true;
  pacer.AddPresentWait(report);
  EXPECT_EQ(1u, pacer.GetPresentWaitTimeouts());
}


TEST(TestSamplePacer, VBlankPeriodOnly_IsThePacerOfTheVerticalBlankTimesWithTheRefreshPeriodOnly)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::VBlankPeriodOnly, 2));

  // Until a vertical blank was read it paces on its clock, and says so
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, pacer.GetTier());

  pacer.AddVBlank(NanosecondTickCount(At(0).Ticks() * NanosecondTickCount::NanosecondsPerTick),
                  NanosecondTimeSpan(PeriodTicks * NanosecondTimeSpan::NanosecondsPerTick), At(PeriodTicks / 8));

  EXPECT_EQ(SamplePacerTier::VBlankPeriodOnly, pacer.GetTier());
}


TEST(TestSamplePacer, VBlankWaitForPresent_IsThePacerOfTheVerticalBlankTimesWithAWaitForAPresent)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::VBlankWaitForPresent, 2);
  config.ReadyPlacePercent = 50;
  SamplePacer pacer(config);

  // Until a vertical blank was read it paces on its clock with the wait, and says so
  EXPECT_EQ(SamplePacerTier::TimerWaitForPresent, pacer.GetTier());
  // A frame is to be ready in the middle of a refresh until the pacer learns otherwise, and no wait has said anything yet
  EXPECT_EQ(TimeSpan(PeriodTicks / 2), pacer.GetReadyPlaceNow());
  EXPECT_EQ(0u, pacer.GetShownLaterByWaits());
  EXPECT_FALSE(pacer.HasVBlankReading());

  pacer.AddVBlank(NanosecondTickCount(At(0).Ticks() * NanosecondTickCount::NanosecondsPerTick),
                  NanosecondTimeSpan(PeriodTicks * NanosecondTimeSpan::NanosecondsPerTick), At(PeriodTicks / 8));
  EXPECT_TRUE(pacer.HasVBlankReading());
  EXPECT_EQ(SamplePacerTier::VBlankWaitForPresent, pacer.GetTier());

  // It names a present to wait for, as the pacer of the timer with a wait does: with two presents that may wait, the one before the last
  const uint64_t firstFrameId = RunFrame(pacer, PeriodTicks / 4);
  RunFrame(pacer, PeriodTicks + (PeriodTicks / 4));
  const SamplePacerFrameStartPlan plan = pacer.PlanFrame(At((2 * PeriodTicks) + (PeriodTicks / 4)));
  EXPECT_EQ(firstFrameId, plan.WaitForPresentFrameId);
}


TEST(TestSamplePacer, TheOtherKindsMoveNoReadyPlace)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  // It is one pacer for every kind, so the place is there: where it was set, and no wait moves it
  for (const SamplePacerKind kind : {SamplePacerKind::TimerPeriodOnly, SamplePacerKind::TimerWaitForPresent, SamplePacerKind::VBlankPeriodOnly})
  {
    SamplePacerConfig config = Config(kind, 2);
    config.ReadyPlacePercent = 50;
    const SamplePacer pacer(config);

    EXPECT_EQ(TimeSpan(PeriodTicks / 2), pacer.GetReadyPlaceNow());
    EXPECT_EQ(0u, pacer.GetShownLaterByWaits());
  }
}


TEST(TestSamplePacer, VBlankPeriodOnly_HasNoVerticalBlankUntilItIsGivenOne)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::VBlankPeriodOnly, 2));
  EXPECT_FALSE(pacer.HasVBlankReading());
  EXPECT_EQ(0u, pacer.GetVBlankJumps());

  // A vertical blank of the display, read a little after it
  const TickCount vblankTime = At(0);
  pacer.AddVBlank(NanosecondTickCount(vblankTime.Ticks() * NanosecondTickCount::NanosecondsPerTick),
                  NanosecondTimeSpan(PeriodTicks * NanosecondTimeSpan::NanosecondsPerTick), At(PeriodTicks / 8));

  EXPECT_TRUE(pacer.HasVBlankReading());
  EXPECT_EQ(0u, pacer.GetVBlankJumps());
  // And it paces frames with it
  const uint64_t firstFrameId = RunFrame(pacer, PeriodTicks / 4);
  const uint64_t secondFrameId = RunFrame(pacer, PeriodTicks + (PeriodTicks / 4));
  EXPECT_NE(0u, firstFrameId);
  EXPECT_NE(firstFrameId, secondFrameId);
}


TEST(TestSamplePacer, VBlankPeriodOnly_AVerticalBlankThatIsNotWhereTheOnesBeforePutItIsCounted)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::VBlankPeriodOnly, 2));
  const NanosecondTimeSpan period(PeriodTicks * NanosecondTimeSpan::NanosecondsPerTick);
  const auto toNanoseconds = [](const TickCount time) { return NanosecondTickCount(time.Ticks() * NanosecondTickCount::NanosecondsPerTick); };

  pacer.AddVBlank(toNanoseconds(At(0)), period, At(100));
  // Ten refreshes later, on the refresh
  pacer.AddVBlank(toNanoseconds(At(10 * PeriodTicks)), period, At((10 * PeriodTicks) + 100));
  EXPECT_EQ(0u, pacer.GetVBlankJumps());

  // And one that is half a refresh off
  pacer.AddVBlank(toNanoseconds(At((20 * PeriodTicks) + (PeriodTicks / 2))), period, At((20 * PeriodTicks) + (PeriodTicks / 2) + 100));
  EXPECT_EQ(1u, pacer.GetVBlankJumps());
}


TEST(TestSamplePacer, TheOtherKindsDoNothingWithAVerticalBlank)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  for (const SamplePacerKind kind : {SamplePacerKind::TimerPeriodOnly, SamplePacerKind::TimerWaitForPresent})
  {
    SamplePacer pacer(Config(kind, 2));

    pacer.AddVBlank(NanosecondTickCount(At(0).Ticks() * NanosecondTickCount::NanosecondsPerTick),
                    NanosecondTimeSpan(PeriodTicks * NanosecondTimeSpan::NanosecondsPerTick), At(100));

    EXPECT_FALSE(pacer.HasVBlankReading());
    EXPECT_EQ(0u, pacer.GetVBlankJumps());
  }
}


TEST(TestSamplePacer, TimerPeriodOnly_AWaitForAFrameSlotThatHeldTheFrameIsCounted)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));
  RunFrame(pacer, 0);
  EXPECT_EQ(0u, pacer.GetFrameSlotHeldFrames());
  EXPECT_EQ(0u, pacer.GetSystemHeldFrames());

  // The wait for the frame slot of the next frame took half a refresh
  pacer.AddSystemWait(SamplePacerSystemWait::FrameSlot, At(PeriodTicks), At(PeriodTicks + (PeriodTicks / 2)));
  // And its acquire returned at once
  pacer.AddSystemWait(SamplePacerSystemWait::Acquire, At(PeriodTicks + (PeriodTicks / 2)), At(PeriodTicks + (PeriodTicks / 2)));
  RunFrame(pacer, PeriodTicks + (PeriodTicks / 2));

  EXPECT_EQ(1u, pacer.GetFrameSlotHeldFrames());
  EXPECT_EQ(0u, pacer.GetSystemHeldFrames());
}


TEST(TestSamplePacer, TimerPeriodOnly_AAcquireThatHeldTheFrameIsCounted)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly));
  RunFrame(pacer, 0);

  pacer.AddSystemWait(SamplePacerSystemWait::FrameSlot, At(PeriodTicks), At(PeriodTicks));
  pacer.AddSystemWait(SamplePacerSystemWait::Acquire, At(PeriodTicks), At(PeriodTicks + (PeriodTicks / 2)));
  RunFrame(pacer, PeriodTicks + (PeriodTicks / 2));

  EXPECT_EQ(0u, pacer.GetFrameSlotHeldFrames());
  EXPECT_EQ(1u, pacer.GetSystemHeldFrames());
}


TEST(TestSamplePacer, TimerWaitForPresent_CountsAWaitOfTheApp)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  // The pacer on a timer takes the waits of the app, with the wait for a present as without it
  SamplePacer pacer(Config(SamplePacerKind::TimerWaitForPresent, 2));
  RunFrame(pacer, 0);

  pacer.AddSystemWait(SamplePacerSystemWait::Acquire, At(PeriodTicks), At(PeriodTicks + (PeriodTicks / 2)));
  RunFrame(pacer, PeriodTicks + (PeriodTicks / 2));

  EXPECT_EQ(0u, pacer.GetFrameSlotHeldFrames());
  EXPECT_EQ(1u, pacer.GetSystemHeldFrames());
}


TEST(TestSamplePacer, TheKindsThatAreNotOnATimerDoNothingWithAWaitOfTheApp)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  for (const SamplePacerKind kind : {SamplePacerKind::VBlankPeriodOnly, SamplePacerKind::VBlankWaitForPresent})
  {
    SamplePacer pacer(Config(kind, 2));
    RunFrame(pacer, 0);

    pacer.AddSystemWait(SamplePacerSystemWait::Acquire, At(PeriodTicks), At(PeriodTicks + (PeriodTicks / 2)));
    RunFrame(pacer, PeriodTicks + (PeriodTicks / 2));

    EXPECT_EQ(0u, pacer.GetFrameSlotHeldFrames());
    EXPECT_EQ(0u, pacer.GetSystemHeldFrames());
  }
}


TEST(TestSamplePacer, SetConfig_AnotherKindIsTheSamePacer)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly, 1));
  EXPECT_EQ(1u, RunFrame(pacer, 0));
  EXPECT_EQ(2u, RunFrame(pacer, PeriodTicks));
  EXPECT_EQ(0u, pacer.PlanFrame(At(PeriodTicks * 2)).WaitForPresentFrameId);

  pacer.SetConfig(Config(SamplePacerKind::TimerWaitForPresent, 1));

  // It waits for a present now: with one present that may wait, the one of the frame before
  EXPECT_EQ(SamplePacerTier::TimerWaitForPresent, pacer.GetTier());
  EXPECT_EQ(2u, pacer.PlanFrame(At(PeriodTicks * 2)).WaitForPresentFrameId);
  // And its frames count on
  EXPECT_EQ(3u, RunFrame(pacer, PeriodTicks * 2));

  pacer.SetConfig(Config(SamplePacerKind::TimerPeriodOnly, 1));

  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, pacer.GetTier());
  EXPECT_EQ(0u, pacer.PlanFrame(At(PeriodTicks * 3)).WaitForPresentFrameId);
  EXPECT_EQ(4u, RunFrame(pacer, PeriodTicks * 3));
}


TEST(TestSamplePacer, SetConfig_AnotherKindIsInForceAfterTheFrameThatIsOpen)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacer pacer(Config(SamplePacerKind::TimerPeriodOnly, 1));
  RunFrame(pacer, 0);
  pacer.BeginFrame(At(PeriodTicks));

  pacer.SetConfig(Config(SamplePacerKind::TimerWaitForPresent, 1));

  // The frame that is open is still the one of the timer alone
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, pacer.GetTier());
  pacer.EndFrame(At(PeriodTicks + (PeriodTicks / 10)));
  EXPECT_EQ(SamplePacerTier::TimerWaitForPresent, pacer.GetTier());
}


TEST(TestSamplePacer, AKindTheAppHasNotEverythingForPacesAsTheKindOfWhatIsLeft)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  // The app can wait for a present and has no vertical blank times
  SamplePacerConfig config = Config(SamplePacerKind::VBlankWaitForPresent, 1);
  config.Capabilities.VBlankTimes = false;
  SamplePacer pacer(config);

  pacer.AddVBlank(NanosecondTickCount(At(0).Ticks() * NanosecondTickCount::NanosecondsPerTick),
                  NanosecondTimeSpan(PeriodTicks * NanosecondTimeSpan::NanosecondsPerTick), At(PeriodTicks / 8));

  EXPECT_FALSE(pacer.HasVBlankReading());
  EXPECT_EQ(SamplePacerTier::TimerWaitForPresent, pacer.GetTier());
  const uint64_t firstFrameId = RunFrame(pacer, 0);
  EXPECT_EQ(firstFrameId, pacer.PlanFrame(At(PeriodTicks)).WaitForPresentFrameId);

  // The app has the vertical blank times now: the kind takes them up, and nothing starts again
  config.Capabilities.VBlankTimes = true;
  pacer.SetConfig(config);
  pacer.AddVBlank(NanosecondTickCount(At(PeriodTicks).Ticks() * NanosecondTickCount::NanosecondsPerTick),
                  NanosecondTimeSpan(PeriodTicks * NanosecondTimeSpan::NanosecondsPerTick), At(PeriodTicks + (PeriodTicks / 8)));

  EXPECT_TRUE(pacer.HasVBlankReading());
  EXPECT_EQ(SamplePacerTier::VBlankWaitForPresent, pacer.GetTier());
  EXPECT_EQ(firstFrameId + 1u, RunFrame(pacer, PeriodTicks + (PeriodTicks / 4)));
}


TEST(TestSamplePacer, TimedPresent_ThePlanHasTheTimeTheFrameBeforeStaysOnScreen)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  // The present of the app takes a time, and the run is asked to use it
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.Capabilities.PresentAfterDuration = true;
  config.TimedPresent = true;
  SamplePacer pacer(config);

  EXPECT_TRUE(pacer.IsTimedPresentInUse());
  // The tier is the one of the kind: the time the frame before stays on screen changes no tier
  EXPECT_EQ(SamplePacerTier::TimerPeriodOnly, pacer.GetTier());
  pacer.BeginFrame(At(0));
  const SamplePacerPresentPlan plan = pacer.EndFrame(At(PeriodTicks / 10));
  // One refresh per frame: the frame before stays for the swap interval less half a refresh
  EXPECT_EQ((PeriodTicks / 2) * NanosecondTimeSpan::NanosecondsPerTick, plan.MinimumDuration.TotalNanoseconds());
}


TEST(TestSamplePacer, TimedPresent_NotAskedFor_ThePlanHasNoSuchTime)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.Capabilities.PresentAfterDuration = true;
  SamplePacer pacer(config);

  EXPECT_FALSE(pacer.IsTimedPresentInUse());
  pacer.BeginFrame(At(0));
  EXPECT_EQ(0, pacer.EndFrame(At(PeriodTicks / 10)).MinimumDuration.TotalNanoseconds());
}


TEST(TestSamplePacer, TimedPresent_AAppWithoutSuchAPresentDoesNotUseIt)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.TimedPresent = true;
  SamplePacer pacer(config);

  EXPECT_FALSE(pacer.IsTimedPresentInUse());
  pacer.BeginFrame(At(0));
  EXPECT_EQ(0, pacer.EndFrame(At(PeriodTicks / 10)).MinimumDuration.TotalNanoseconds());
}


TEST(TestSamplePacer, SetConfig_TimedPresentIsTheSamePacer)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.Capabilities.PresentAfterDuration = true;
  SamplePacer pacer(config);
  EXPECT_EQ(1u, RunFrame(pacer, 0));
  EXPECT_FALSE(pacer.IsTimedPresentInUse());

  config.TimedPresent = true;
  pacer.SetConfig(config);

  EXPECT_TRUE(pacer.IsTimedPresentInUse());
  // Its frames count on
  EXPECT_EQ(2u, RunFrame(pacer, PeriodTicks));
}


TEST(TestSamplePacer, DisplayReports_ThePacerCountsTheAnimationErrorFromThem)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  // The app measures its presents, and the run gives the pacer the display times
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.Capabilities.DisplayTimes = true;
  config.DisplayReports = true;
  SamplePacer pacer(config);
  const uint64_t firstFrameId = RunFrame(pacer, 0);
  const uint64_t secondFrameId = RunFrame(pacer, PeriodTicks);
  const uint64_t thirdFrameId = RunFrame(pacer, PeriodTicks * 2);

  // The first two are shown a refresh apart, the third two refreshes after the second: a refresh late
  pacer.AddDisplayTime(firstFrameId, At(PeriodTicks));
  pacer.AddDisplayTime(secondFrameId, At(PeriodTicks * 2));
  pacer.AddDisplayTime(thirdFrameId, At(PeriodTicks * 4));

  const SamplePacerDisplayErrors errors = pacer.GetDisplayErrors();
  EXPECT_EQ(3u, errors.Reports);
  EXPECT_EQ(0u, errors.Refused);
  // A frame is judged against the frame before it, so the first is not
  EXPECT_EQ(2u, errors.JudgedFrames);
  EXPECT_EQ(1u, errors.ErrorFrames);
  EXPECT_EQ(1u, errors.OffTargetFrames);
  EXPECT_EQ(1u, errors.LateFrames);
}


TEST(TestSamplePacer, DisplayReports_NotAskedFor_NothingIsCounted)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.Capabilities.DisplayTimes = true;
  SamplePacer pacer(config);
  const uint64_t firstFrameId = RunFrame(pacer, 0);

  pacer.AddDisplayTime(firstFrameId, At(PeriodTicks));

  EXPECT_EQ(0u, pacer.GetDisplayErrors().Reports);
}


TEST(TestSamplePacer, ReadyPlaceTries_ANewPacerHasTriedNothing)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  for (const SamplePacerKind kind : {SamplePacerKind::TimerPeriodOnly, SamplePacerKind::VBlankWaitForPresent})
  {
    const SamplePacer pacer(Config(kind, 2));

    EXPECT_EQ(0u, pacer.GetReadyPlaceTries());
    EXPECT_EQ(0u, pacer.GetReadyPlaceTriesTakenBack());
  }
}


TEST(TestSamplePacer, GpuWait_WithLowLatencyThePlanNamesTheFrameBefore)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.Capabilities.WaitForGpuWork = true;
  config.GpuWait = true;
  SamplePacer pacer(config);

  // Nothing to wait for before a frame was made
  EXPECT_EQ(0u, pacer.PlanFrame(At(0)).WaitForGpuWorkFrameId);

  const uint64_t firstFrameId = RunFrame(pacer, 0);
  const SamplePacerFrameStartPlan plan = pacer.PlanFrame(At(PeriodTicks / 4));
  EXPECT_EQ(firstFrameId, plan.WaitForGpuWorkFrameId);
  EXPECT_GT(plan.WaitForGpuWorkTimeout.Ticks(), 0);
  EXPECT_EQ(0u, plan.WaitForPresentFrameId);

  // After the wait the frame is not asked for again
  SamplePacerGpuWaitReport report;
  report.FrameId = firstFrameId;
  report.BeginTime = At(PeriodTicks / 4);
  report.EndTime = At(PeriodTicks / 2);
  pacer.AddGpuWait(report);
  EXPECT_EQ(0u, pacer.PlanFrame(At(PeriodTicks / 2)).WaitForGpuWorkFrameId);
  EXPECT_EQ(0u, pacer.GetGpuWaitTimeouts());
}


TEST(TestSamplePacer, GpuWait_WithSmoothnessAndTwoFramesInFlightThePlanNamesTheFrameBeforeThat)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly, 2, SamplePacerAim::Smoothness);
  config.Capabilities.WaitForGpuWork = true;
  config.GpuWait = true;
  config.MaxFramesInFlight = 2;
  SamplePacer pacer(config);

  const uint64_t firstFrameId = RunFrame(pacer, 0);
  // The frame before that of the second frame is none
  EXPECT_EQ(0u, pacer.PlanFrame(At(PeriodTicks / 2)).WaitForGpuWorkFrameId);
  RunFrame(pacer, PeriodTicks);
  EXPECT_EQ(firstFrameId, pacer.PlanFrame(At(PeriodTicks + (PeriodTicks / 2))).WaitForGpuWorkFrameId);
}


TEST(TestSamplePacer, GpuWait_WithSmoothnessAndOneFrameInFlightThePlanNamesTheFrameBefore)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly, 2, SamplePacerAim::Smoothness);
  config.Capabilities.WaitForGpuWork = true;
  config.GpuWait = true;
  config.MaxFramesInFlight = 1;
  SamplePacer pacer(config);

  const uint64_t firstFrameId = RunFrame(pacer, 0);
  EXPECT_EQ(firstFrameId, pacer.PlanFrame(At(PeriodTicks / 2)).WaitForGpuWorkFrameId);
}


TEST(TestSamplePacer, GpuWait_NotAskedForWithoutTheConfigWithoutTheCapabilityOrNextToAWaitForAPresent)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  {    // The app can make the wait, the config does not ask for it
    SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
    config.Capabilities.WaitForGpuWork = true;
    SamplePacer pacer(config);
    RunFrame(pacer, 0);
    EXPECT_EQ(0u, pacer.PlanFrame(At(PeriodTicks / 4)).WaitForGpuWorkFrameId);
  }
  {    // The config asks for it, the app can not make the wait
    SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
    config.GpuWait = true;
    SamplePacer pacer(config);
    RunFrame(pacer, 0);
    EXPECT_EQ(0u, pacer.PlanFrame(At(PeriodTicks / 4)).WaitForGpuWorkFrameId);
  }
  {    // A pacer that waits for a present holds the loop with that wait
    SamplePacerConfig config = Config(SamplePacerKind::TimerWaitForPresent, 1);
    config.Capabilities.WaitForGpuWork = true;
    config.GpuWait = true;
    SamplePacer pacer(config);
    const uint64_t firstFrameId = RunFrame(pacer, 0);
    const SamplePacerFrameStartPlan plan = pacer.PlanFrame(At(PeriodTicks / 4));
    EXPECT_EQ(0u, plan.WaitForGpuWorkFrameId);
    EXPECT_EQ(firstFrameId, plan.WaitForPresentFrameId);
  }
}


TEST(TestSamplePacer, GpuWait_AWaitThatRanOutIsCounted)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  SamplePacerConfig config = Config(SamplePacerKind::TimerPeriodOnly);
  config.Capabilities.WaitForGpuWork = true;
  config.GpuWait = true;
  SamplePacer pacer(config);
  const uint64_t firstFrameId = RunFrame(pacer, 0);
  const SamplePacerFrameStartPlan plan = pacer.PlanFrame(At(PeriodTicks / 4));
  ASSERT_EQ(firstFrameId, plan.WaitForGpuWorkFrameId);

  SamplePacerGpuWaitReport report;
  report.FrameId = firstFrameId;
  report.BeginTime = At(PeriodTicks / 4);
  report.EndTime = At(PeriodTicks / 4) + plan.WaitForGpuWorkTimeout;
  report.Done = false;
  pacer.AddGpuWait(report);

  EXPECT_EQ(1u, pacer.GetGpuWaitTimeouts());
}


TEST(TestSamplePacer, DisplayHeldRefreshes_ANewPacerHasNone)
{
  if (!SamplePacer::IsSupported())
  {
    return;
  }
  for (const SamplePacerKind kind : {SamplePacerKind::TimerPeriodOnly, SamplePacerKind::VBlankPeriodOnly, SamplePacerKind::VBlankWaitForPresent})
  {
    const SamplePacer pacer(Config(kind, 2));

    EXPECT_EQ(0u, pacer.GetDisplayHeldRefreshes());
  }
}
