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

#include <FslBase/Time/TimeSpan.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <Shared/FramePacing/SampleFrameWorkAverage.hpp>

using namespace Fsl;

namespace
{
  using TestSampleFrameWorkAverage = TestFixtureFslBase;

  SampleFrameWorkRecord Record(const int64_t cpuTicks, const int64_t gpuTicks, const int64_t frameTicks)
  {
    SampleFrameWorkRecord record;
    record.CpuTime = TimeSpan(cpuTicks);
    record.GpuTime = TimeSpan(gpuTicks);
    record.FrameTime = TimeSpan(frameTicks);
    return record;
  }
}


TEST(TestSampleFrameWorkAverage, Empty_AllZero)
{
  const SampleFrameWorkAverage average;

  EXPECT_EQ(0u, average.Count());
  EXPECT_EQ(SampleFrameWorkRecord(), average.CalcAverage());
}


TEST(TestSampleFrameWorkAverage, TheAverageOfTheFrames)
{
  SampleFrameWorkAverage average;
  average.Add(Record(100, 200, 400));
  average.Add(Record(300, 400, 800));

  EXPECT_EQ(2u, average.Count());
  EXPECT_EQ(Record(200, 300, 600), average.CalcAverage());
}


TEST(TestSampleFrameWorkAverage, AFrameWithoutAGpuTimeIsLeftOutOfTheGpuAverage)
{
  SampleFrameWorkAverage average;
  average.Add(Record(100, 0, 100));
  average.Add(Record(100, 600, 700));
  average.Add(Record(100, 200, 300));

  const SampleFrameWorkRecord result = average.CalcAverage();

  EXPECT_EQ(TimeSpan(100), result.CpuTime);
  EXPECT_EQ(TimeSpan(400), result.GpuTime);
  // (100 + 700 + 300) / 3
  EXPECT_EQ(TimeSpan(366), result.FrameTime);
}


TEST(TestSampleFrameWorkAverage, NoGpuTimeAtAll_TheGpuAverageIsZero)
{
  SampleFrameWorkAverage average;
  average.Add(Record(100, 0, 100));
  average.Add(Record(300, 0, 300));

  EXPECT_EQ(Record(200, 0, 200), average.CalcAverage());
}


TEST(TestSampleFrameWorkAverage, OnlyTheLastFramesCount)
{
  SampleFrameWorkAverage average;
  average.Add(Record(1000000, 1000000, 1000000));
  for (std::size_t i = 0; i < SampleFrameWorkAverage::Capacity; ++i)
  {
    average.Add(Record(100, 200, 300));
  }

  EXPECT_EQ(SampleFrameWorkAverage::Capacity, average.Count());
  EXPECT_EQ(Record(100, 200, 300), average.CalcAverage());
}


TEST(TestSampleFrameWorkAverage, Clear_ForgetsTheFrames)
{
  SampleFrameWorkAverage average;
  average.Add(Record(100, 200, 300));

  average.Clear();

  EXPECT_EQ(0u, average.Count());
  EXPECT_EQ(SampleFrameWorkRecord(), average.CalcAverage());
}
