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
#include <FslDemoService/FramePacing/Impl/FramePacingSequence.hpp>

using namespace Fsl;

namespace
{
  using Test_FramePacingSequence = TestFixtureFslBase;

  constexpr TickCount Ms(const int64_t milliseconds) noexcept
  {
    return TickCount(milliseconds * TickCount::TicksPerMillisecond);
  }

  constexpr TimeSpan MsSpan(const int64_t milliseconds) noexcept
  {
    return TimeSpan(milliseconds * TimeSpan::TicksPerMillisecond);
  }
}


TEST(Test_FramePacingSequence, Construct)
{
  FramePacingSequence sequence;

  EXPECT_EQ(FramePacingRunState::Idle, sequence.GetState());
  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(0)));
  EXPECT_EQ(FramePacingRunState::Idle, sequence.GetState());
}


TEST(Test_FramePacingSequence, BeginRun_WhileActive)
{
  FramePacingSequence sequence;

  EXPECT_TRUE(sequence.BeginRun(TimeSpan()));
  EXPECT_FALSE(sequence.BeginRun(TimeSpan()));
  EXPECT_EQ(FramePacingRunState::Starting, sequence.GetState());
}


TEST(Test_FramePacingSequence, FullRun)
{
  FramePacingSequence sequence(MsSpan(100), MsSpan(50));

  // Begin the run long before the first frame, the start marker time is measured from the first frame it is shown in
  EXPECT_TRUE(sequence.BeginRun(MsSpan(1000)));

  EXPECT_EQ(FramePacingMarkerKind::SequenceStart, sequence.Advance(Ms(5000)));
  EXPECT_EQ(FramePacingMarkerKind::SequenceStart, sequence.Advance(Ms(5050)));
  EXPECT_EQ(FramePacingMarkerKind::SequenceStart, sequence.Advance(Ms(5099)));
  EXPECT_EQ(FramePacingRunState::Starting, sequence.GetState());

  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(5100)));
  EXPECT_EQ(FramePacingRunState::Measuring, sequence.GetState());
  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(6099)));
  EXPECT_EQ(FramePacingRunState::Measuring, sequence.GetState());

  EXPECT_EQ(FramePacingMarkerKind::SequenceEnd, sequence.Advance(Ms(6100)));
  EXPECT_EQ(FramePacingRunState::Ending, sequence.GetState());
  EXPECT_EQ(FramePacingMarkerKind::SequenceEnd, sequence.Advance(Ms(6149)));

  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(6150)));
  EXPECT_EQ(FramePacingRunState::Idle, sequence.GetState());

  // A new run can be started once the old one completed
  EXPECT_TRUE(sequence.BeginRun(TimeSpan()));
}


TEST(Test_FramePacingSequence, ZeroMarkerDuration_ShownForOneFrame)
{
  FramePacingSequence sequence{TimeSpan(), TimeSpan()};

  EXPECT_TRUE(sequence.BeginRun(TimeSpan()));
  EXPECT_EQ(FramePacingMarkerKind::SequenceStart, sequence.Advance(Ms(10)));
  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(10)));
  EXPECT_EQ(FramePacingRunState::Measuring, sequence.GetState());

  sequence.EndRun();
  EXPECT_EQ(FramePacingMarkerKind::SequenceEnd, sequence.Advance(Ms(20)));
  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(20)));
  EXPECT_EQ(FramePacingRunState::Idle, sequence.GetState());
}


TEST(Test_FramePacingSequence, ZeroMeasureDuration_UntilEndRun)
{
  FramePacingSequence sequence(MsSpan(10), MsSpan(10));

  EXPECT_TRUE(sequence.BeginRun(TimeSpan()));
  EXPECT_EQ(FramePacingMarkerKind::SequenceStart, sequence.Advance(Ms(0)));
  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(10)));
  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(1000000)));
  EXPECT_EQ(FramePacingRunState::Measuring, sequence.GetState());

  sequence.EndRun();
  EXPECT_EQ(FramePacingRunState::Ending, sequence.GetState());
  EXPECT_EQ(FramePacingMarkerKind::SequenceEnd, sequence.Advance(Ms(1000001)));
}


TEST(Test_FramePacingSequence, EndRun_WhileStarting)
{
  FramePacingSequence sequence(MsSpan(100), MsSpan(100));

  EXPECT_TRUE(sequence.BeginRun(TimeSpan()));
  EXPECT_EQ(FramePacingMarkerKind::SequenceStart, sequence.Advance(Ms(0)));
  sequence.EndRun();
  EXPECT_EQ(FramePacingRunState::Ending, sequence.GetState());
  EXPECT_EQ(FramePacingMarkerKind::SequenceEnd, sequence.Advance(Ms(10)));
}


TEST(Test_FramePacingSequence, EndRun_WhileIdle)
{
  FramePacingSequence sequence;

  sequence.EndRun();
  EXPECT_EQ(FramePacingRunState::Idle, sequence.GetState());
  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(0)));
}


TEST(Test_FramePacingSequence, EndRun_WhileEnding_DoesNotRestartEndMarker)
{
  FramePacingSequence sequence(MsSpan(0), MsSpan(100));

  EXPECT_TRUE(sequence.BeginRun(TimeSpan()));
  EXPECT_EQ(FramePacingMarkerKind::SequenceStart, sequence.Advance(Ms(0)));
  sequence.EndRun();
  EXPECT_EQ(FramePacingMarkerKind::SequenceEnd, sequence.Advance(Ms(0)));
  sequence.EndRun();
  EXPECT_EQ(FramePacingMarkerKind::SequenceEnd, sequence.Advance(Ms(50)));
  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(100)));
  EXPECT_EQ(FramePacingRunState::Idle, sequence.GetState());
}


TEST(Test_FramePacingSequence, MeasuredTime)
{
  FramePacingSequence sequence(MsSpan(100), MsSpan(100));

  EXPECT_EQ(TimeSpan(), sequence.GetMeasuredTime(Ms(0)));
  EXPECT_TRUE(sequence.BeginRun(MsSpan(1000)));
  EXPECT_EQ(MsSpan(1000), sequence.GetMeasureDuration());

  EXPECT_EQ(FramePacingMarkerKind::SequenceStart, sequence.Advance(Ms(0)));
  // Not measuring while the start marker is shown
  EXPECT_EQ(TimeSpan(), sequence.GetMeasuredTime(Ms(50)));

  EXPECT_EQ(FramePacingMarkerKind::Frame, sequence.Advance(Ms(100)));
  EXPECT_EQ(TimeSpan(), sequence.GetMeasuredTime(Ms(100)));
  EXPECT_EQ(MsSpan(400), sequence.GetMeasuredTime(Ms(500)));
  // Capped to the duration until the next frame ends the measurement
  EXPECT_EQ(MsSpan(1000), sequence.GetMeasuredTime(Ms(5000)));

  EXPECT_EQ(FramePacingMarkerKind::SequenceEnd, sequence.Advance(Ms(1100)));
  EXPECT_EQ(TimeSpan(), sequence.GetMeasuredTime(Ms(1100)));
  // The duration of the last run is still available
  EXPECT_EQ(MsSpan(1000), sequence.GetMeasureDuration());
}
