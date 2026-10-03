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
#include <FslDemoService/SystemStats/Impl/Adapter/Win32/GpuProcessUsageAggregator.hpp>

using namespace Fsl;

namespace
{
  using Test_GpuProcessUsageAggregator = TestFixtureFslBase;

  constexpr uint32_t ProcessId = 1234;
}


TEST_F(Test_GpuProcessUsageAggregator, IsProcessInstance)
{
  EXPECT_TRUE(GpuProcessUsageAggregator::IsProcessInstance("pid_1234_luid_0x00000000_0x00012014_phys_0_eng_0_engtype_3D", ProcessId));
  EXPECT_TRUE(GpuProcessUsageAggregator::IsProcessInstance("pid_1234_luid_0x00000000_0x00012014_phys_0", ProcessId));
  EXPECT_TRUE(GpuProcessUsageAggregator::IsProcessInstance("pid_1234_", ProcessId));
}


TEST_F(Test_GpuProcessUsageAggregator, IsProcessInstance_OtherProcess)
{
  // The id has to match as a whole
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("pid_12345_luid_0x00000000_0x00012014_phys_0", ProcessId));
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("pid_123_luid_0x00000000_0x00012014_phys_0", ProcessId));
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("pid_1234_luid", 123u));
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("pid_4321_luid", ProcessId));
}


TEST_F(Test_GpuProcessUsageAggregator, IsProcessInstance_NotAInstanceName)
{
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("", ProcessId));
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("pid_", ProcessId));
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("pid_1234", ProcessId));
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("pid__1234_", ProcessId));
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("1234_luid", ProcessId));
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("_Total", ProcessId));
  EXPECT_FALSE(GpuProcessUsageAggregator::IsProcessInstance("pid_99999999999999999999_", ProcessId));
}


TEST_F(Test_GpuProcessUsageAggregator, Empty)
{
  const GpuProcessUsageAggregator aggregator(ProcessId);

  float usage = 42.0f;
  EXPECT_FALSE(aggregator.TryGetUsagePercentage(usage));
  EXPECT_EQ(0.0f, usage);

  uint64_t dedicated = 42;
  uint64_t shared = 42;
  EXPECT_FALSE(aggregator.TryGetMemoryUsage(dedicated, shared));
  EXPECT_EQ(0u, dedicated);
  EXPECT_EQ(0u, shared);
}


TEST_F(Test_GpuProcessUsageAggregator, Usage_IsTheBusiestEngine)
{
  GpuProcessUsageAggregator aggregator(ProcessId);
  // The engines of a process as Windows lists them, on two GPUs
  aggregator.AddEngineUtilization("pid_1234_luid_0x00000000_0x00012014_phys_0_eng_0_engtype_3D", 40.5);
  aggregator.AddEngineUtilization("pid_1234_luid_0x00000000_0x00012014_phys_0_eng_1_engtype_Copy", 3.0);
  aggregator.AddEngineUtilization("pid_1234_luid_0x00000000_0x00012014_phys_0_eng_10_engtype_JPEG_Decode_3", 0.0);
  aggregator.AddEngineUtilization("pid_1234_luid_0x00000000_0x000136BA_phys_0_eng_0_engtype_3D", 55.25);

  float usage = 0.0f;
  ASSERT_TRUE(aggregator.TryGetUsagePercentage(usage));
  // Not the sum: that is what the Task Manager shows as well
  EXPECT_FLOAT_EQ(55.25f, usage);
}


TEST_F(Test_GpuProcessUsageAggregator, Usage_OtherProcessesAreIgnored)
{
  GpuProcessUsageAggregator aggregator(ProcessId);
  aggregator.AddEngineUtilization("pid_12345_luid_0x00000000_0x00012014_phys_0_eng_0_engtype_3D", 90.0);
  aggregator.AddEngineUtilization("pid_123_luid_0x00000000_0x00012014_phys_0_eng_0_engtype_3D", 80.0);

  float usage = 0.0f;
  EXPECT_FALSE(aggregator.TryGetUsagePercentage(usage));

  aggregator.AddEngineUtilization("pid_1234_luid_0x00000000_0x00012014_phys_0_eng_0_engtype_3D", 10.0);
  ASSERT_TRUE(aggregator.TryGetUsagePercentage(usage));
  EXPECT_FLOAT_EQ(10.0f, usage);
}


TEST_F(Test_GpuProcessUsageAggregator, Usage_BadValues)
{
  GpuProcessUsageAggregator aggregator(ProcessId);
  aggregator.AddEngineUtilization("pid_1234_a", std::numeric_limits<double>::quiet_NaN());
  aggregator.AddEngineUtilization("pid_1234_b", std::numeric_limits<double>::infinity());
  aggregator.AddEngineUtilization("pid_1234_c", -5.0);

  float usage = 0.0f;
  EXPECT_FALSE(aggregator.TryGetUsagePercentage(usage));

  // A counter can go over 100 for a moment
  aggregator.AddEngineUtilization("pid_1234_d", 135.0);
  ASSERT_TRUE(aggregator.TryGetUsagePercentage(usage));
  EXPECT_FLOAT_EQ(100.0f, usage);
}


TEST_F(Test_GpuProcessUsageAggregator, Memory_IsTheSumOverTheGpus)
{
  GpuProcessUsageAggregator aggregator(ProcessId);
  aggregator.AddDedicatedUsage("pid_1234_luid_0x00000000_0x00012014_phys_0", 1000);
  aggregator.AddDedicatedUsage("pid_1234_luid_0x00000000_0x000136BA_phys_0", 200);
  aggregator.AddSharedUsage("pid_1234_luid_0x00000000_0x00012014_phys_0", 30);
  aggregator.AddSharedUsage("pid_1234_luid_0x00000000_0x000136BA_phys_0", 4);
  // Another process
  aggregator.AddDedicatedUsage("pid_12345_luid_0x00000000_0x00012014_phys_0", 50000);
  aggregator.AddSharedUsage("pid_123_luid_0x00000000_0x00012014_phys_0", 60000);

  uint64_t dedicated = 0;
  uint64_t shared = 0;
  ASSERT_TRUE(aggregator.TryGetMemoryUsage(dedicated, shared));
  EXPECT_EQ(1200u, dedicated);
  EXPECT_EQ(34u, shared);
}


TEST_F(Test_GpuProcessUsageAggregator, Memory_OnlyOneKind)
{
  GpuProcessUsageAggregator aggregator(ProcessId);
  aggregator.AddSharedUsage("pid_1234_luid_0x00000000_0x00012014_phys_0", 30);

  uint64_t dedicated = 42;
  uint64_t shared = 0;
  ASSERT_TRUE(aggregator.TryGetMemoryUsage(dedicated, shared));
  EXPECT_EQ(0u, dedicated);
  EXPECT_EQ(30u, shared);
}


TEST_F(Test_GpuProcessUsageAggregator, Clear)
{
  GpuProcessUsageAggregator aggregator(ProcessId);
  aggregator.AddEngineUtilization("pid_1234_a", 50.0);
  aggregator.AddDedicatedUsage("pid_1234_a", 1000);
  aggregator.Clear();

  float usage = 0.0f;
  uint64_t dedicated = 0;
  uint64_t shared = 0;
  EXPECT_FALSE(aggregator.TryGetUsagePercentage(usage));
  EXPECT_FALSE(aggregator.TryGetMemoryUsage(dedicated, shared));
}
