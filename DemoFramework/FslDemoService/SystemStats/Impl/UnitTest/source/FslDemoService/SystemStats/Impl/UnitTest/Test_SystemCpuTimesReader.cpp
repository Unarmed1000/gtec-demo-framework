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
#include <FslDemoService/SystemStats/Impl/Adapter/SystemCpuTimesReader.hpp>

using namespace Fsl;

namespace
{
  using Test_SystemCpuTimesReader = TestFixtureFslBase;

  //! The usual unit of /proc/stat is a hundredth of a second
  constexpr uint64_t TicksPerJiffy = 100000;
}


TEST_F(Test_SystemCpuTimesReader, TryParseProcStatCpuLine)
{
  SystemCpuTimes times;

  //                                                              user nice system idle  iowait irq softirq steal guest
  ASSERT_TRUE(SystemCpuTimesReader::TryParseProcStatCpuLine("cpu  1000 20   300    40000 500    6   70      8     9 0", TicksPerJiffy, times));

  EXPECT_EQ((1000u + 20u) * TicksPerJiffy, times.SystemUserTicks);
  EXPECT_EQ((300u + 6u + 70u + 8u) * TicksPerJiffy, times.SystemKernelTicks);
  EXPECT_EQ((40000u + 500u) * TicksPerJiffy, times.SystemIdleTicks);
}


TEST_F(Test_SystemCpuTimesReader, TryParseProcStatCpuLine_OldKernelWithFourFields)
{
  SystemCpuTimes times;

  ASSERT_TRUE(SystemCpuTimesReader::TryParseProcStatCpuLine("cpu 10 2 3 400", TicksPerJiffy, times));

  EXPECT_EQ(12u * TicksPerJiffy, times.SystemUserTicks);
  EXPECT_EQ(3u * TicksPerJiffy, times.SystemKernelTicks);
  EXPECT_EQ(400u * TicksPerJiffy, times.SystemIdleTicks);
}


TEST_F(Test_SystemCpuTimesReader, TryParseProcStatCpuLine_NotTheCpuLine)
{
  SystemCpuTimes times;
  times.SystemUserTicks = 42;

  // The line of one CPU, another line, too few numbers, nothing
  EXPECT_FALSE(SystemCpuTimesReader::TryParseProcStatCpuLine("cpu0 1000 20 300 40000", TicksPerJiffy, times));
  EXPECT_FALSE(SystemCpuTimesReader::TryParseProcStatCpuLine("intr 1000 20 300 40000", TicksPerJiffy, times));
  EXPECT_FALSE(SystemCpuTimesReader::TryParseProcStatCpuLine("cpu  1000 20 300", TicksPerJiffy, times));
  EXPECT_FALSE(SystemCpuTimesReader::TryParseProcStatCpuLine("cpu  ", TicksPerJiffy, times));
  EXPECT_FALSE(SystemCpuTimesReader::TryParseProcStatCpuLine("", TicksPerJiffy, times));
  EXPECT_FALSE(SystemCpuTimesReader::TryParseProcStatCpuLine("cpu  1000 20 300 40000", 0, times));
  EXPECT_EQ(0u, times.SystemUserTicks);
}


#if defined(_WIN32) || defined(__linux__)

TEST_F(Test_SystemCpuTimesReader, TryRead_TheCountersOnlyGrow)
{
  SystemCpuTimes first;
  ASSERT_TRUE(SystemCpuTimesReader::TryRead(first));

  // Use some CPU time, so this process has something to count
  volatile uint64_t sum = 0;
  for (uint64_t i = 0; i < 20000000u; ++i)
  {
    sum = sum + i;
  }

  SystemCpuTimes second;
  ASSERT_TRUE(SystemCpuTimesReader::TryRead(second));

  // A system that is up has been idle and busy
  EXPECT_GT(first.SystemIdleTicks, 0u);
  EXPECT_GT(first.SystemUserTicks, 0u);
  EXPECT_GE(second.SystemIdleTicks, first.SystemIdleTicks);
  EXPECT_GE(second.SystemKernelTicks, first.SystemKernelTicks);
  EXPECT_GE(second.SystemUserTicks, first.SystemUserTicks);
  EXPECT_GE(second.ProcessKernelTicks, first.ProcessKernelTicks);
  EXPECT_GE(second.ProcessUserTicks, first.ProcessUserTicks);
  EXPECT_GT(second.ProcessKernelTicks + second.ProcessUserTicks, 0u);
}

#endif
