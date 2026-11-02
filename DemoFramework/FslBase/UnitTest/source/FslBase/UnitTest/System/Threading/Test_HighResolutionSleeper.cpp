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

#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/System/Threading/HighResolutionSleeper.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>

using namespace Fsl;

namespace
{
  using TestSystem_Threading_HighResolutionSleeper = TestFixtureFslBase;
}


TEST(TestSystem_Threading_HighResolutionSleeper, WakeMargin_IsAboveZero)
{
  EXPECT_GT(HighResolutionSleeper::GetWakeMargin().Ticks(), 0);
}


TEST(TestSystem_Threading_HighResolutionSleeper, MethodName_IsNotEmpty)
{
  EXPECT_FALSE(HighResolutionSleeper::GetMethodName().empty());
}


TEST(TestSystem_Threading_HighResolutionSleeper, SleepUntil_ATimeThatHasPassedReturns)
{
  const HighResolutionTimer timer;
  HighResolutionSleeper sleeper;
  sleeper.SleepUntil(TickCount());
  sleeper.SleepUntil(timer.GetTimestamp() - TimeSpan(TimeSpan::TicksPerMillisecond));
}


TEST(TestSystem_Threading_HighResolutionSleeper, SleepUntil_DoesNotWakeBeforeTheMargin)
{
  const HighResolutionTimer timer;
  HighResolutionSleeper sleeper;
  const TimeSpan margin = HighResolutionSleeper::GetWakeMargin();
  const TickCount time = timer.GetTimestamp() + TimeSpan(2 * TimeSpan::TicksPerMillisecond) + margin;

  sleeper.SleepUntil(time);

  // Only the early side: how late a sleep wakes depends on what else the machine is doing
  EXPECT_GE((timer.GetTimestamp() - (time - margin)).Ticks(), 0);
}
