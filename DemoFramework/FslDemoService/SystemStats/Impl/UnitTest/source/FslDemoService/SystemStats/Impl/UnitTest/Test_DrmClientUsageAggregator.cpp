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
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/Linux/DrmClientUsageAggregator.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/Linux/DrmFdInfoParser.hpp>
#include <fmt/format.h>

using namespace Fsl;

namespace
{
  using Test_DrmClientUsageAggregator = TestFixtureFslBase;

  constexpr TickCount Time0(TimeSpan::TicksPerSecond * 100);
  constexpr TickCount Time1(TimeSpan::TicksPerSecond * 101);

  DrmFdInfo CreateClient(const uint64_t clientId, const uint64_t renderNanoseconds, const uint64_t copyNanoseconds, const uint64_t memoryKiB)
  {
    return DrmFdInfoParser::Parse(fmt::format("drm-client-id: {}\ndrm-engine-render: {} ns\ndrm-engine-copy: {} ns\ndrm-resident-memory: {} KiB\n",
                                              clientId, renderNanoseconds, copyNanoseconds, memoryKiB));
  }

  void Sample(DrmClientUsageAggregator& rAggregator, const TickCount time, const std::initializer_list<DrmFdInfo> clients)
  {
    rAggregator.BeginSample();
    for (const DrmFdInfo& client : clients)
    {
      rAggregator.AddClient(client);
    }
    rAggregator.EndSample(time);
  }
}


TEST_F(Test_DrmClientUsageAggregator, Empty)
{
  const DrmClientUsageAggregator aggregator;

  float usage = 42.0f;
  uint64_t bytes = 42;
  EXPECT_FALSE(aggregator.TryGetUsagePercentage(usage));
  EXPECT_FALSE(aggregator.TryGetMemoryBytes(bytes));
  EXPECT_EQ(0.0f, usage);
  EXPECT_EQ(0u, bytes);
}


TEST_F(Test_DrmClientUsageAggregator, OneSample_MemoryButNoUsageYet)
{
  DrmClientUsageAggregator aggregator;
  Sample(aggregator, Time0, {CreateClient(1, 1000, 0, 64)});

  float usage = 0.0f;
  uint64_t bytes = 0;
  EXPECT_FALSE(aggregator.TryGetUsagePercentage(usage));
  ASSERT_TRUE(aggregator.TryGetMemoryBytes(bytes));
  EXPECT_EQ(64u * 1024u, bytes);
}


TEST_F(Test_DrmClientUsageAggregator, TwoSamples_UsageIsTheBusiestEngine)
{
  DrmClientUsageAggregator aggregator;
  Sample(aggregator, Time0, {CreateClient(1, 1000000000, 500000000, 64)});
  // One second later: the render engine worked 250 ms and the copy engine 100 ms
  Sample(aggregator, Time1, {CreateClient(1, 1250000000, 600000000, 128)});

  float usage = 0.0f;
  uint64_t bytes = 0;
  ASSERT_TRUE(aggregator.TryGetUsagePercentage(usage));
  EXPECT_NEAR(25.0f, usage, 0.001f);
  ASSERT_TRUE(aggregator.TryGetMemoryBytes(bytes));
  EXPECT_EQ(128u * 1024u, bytes);
}


TEST_F(Test_DrmClientUsageAggregator, TwoClients_AreSummed)
{
  DrmClientUsageAggregator aggregator;
  Sample(aggregator, Time0, {CreateClient(1, 0, 0, 10), CreateClient(2, 0, 0, 20)});
  Sample(aggregator, Time1, {CreateClient(1, 100000000, 0, 10), CreateClient(2, 300000000, 0, 20)});

  float usage = 0.0f;
  uint64_t bytes = 0;
  ASSERT_TRUE(aggregator.TryGetUsagePercentage(usage));
  EXPECT_NEAR(40.0f, usage, 0.001f);
  ASSERT_TRUE(aggregator.TryGetMemoryBytes(bytes));
  EXPECT_EQ(30u * 1024u, bytes);
}


TEST_F(Test_DrmClientUsageAggregator, TheSameClientTwice_IsCountedOnce)
{
  DrmClientUsageAggregator aggregator;
  // Two file descriptors of the same client
  Sample(aggregator, Time0, {CreateClient(1, 0, 0, 10), CreateClient(1, 0, 0, 10)});
  Sample(aggregator, Time1, {CreateClient(1, 100000000, 0, 10), CreateClient(1, 100000000, 0, 10)});

  float usage = 0.0f;
  uint64_t bytes = 0;
  ASSERT_TRUE(aggregator.TryGetUsagePercentage(usage));
  EXPECT_NEAR(10.0f, usage, 0.001f);
  ASSERT_TRUE(aggregator.TryGetMemoryBytes(bytes));
  EXPECT_EQ(10u * 1024u, bytes);
}


TEST_F(Test_DrmClientUsageAggregator, NotAClient_IsIgnored)
{
  DrmClientUsageAggregator aggregator;
  Sample(aggregator, Time0, {DrmFdInfo()});
  Sample(aggregator, Time1, {DrmFdInfo()});

  float usage = 0.0f;
  uint64_t bytes = 0;
  EXPECT_FALSE(aggregator.TryGetUsagePercentage(usage));
  EXPECT_FALSE(aggregator.TryGetMemoryBytes(bytes));
}


TEST_F(Test_DrmClientUsageAggregator, UsageIsClamped)
{
  DrmClientUsageAggregator aggregator;
  Sample(aggregator, Time0, {CreateClient(1, 0, 0, 10)});
  // More work than time: several engines of the same kind
  Sample(aggregator, Time1, {CreateClient(1, 3000000000, 0, 10)});

  float usage = 0.0f;
  ASSERT_TRUE(aggregator.TryGetUsagePercentage(usage));
  EXPECT_FLOAT_EQ(100.0f, usage);
}


TEST_F(Test_DrmClientUsageAggregator, BusyTimeThatWentBack_HasNoUsage)
{
  DrmClientUsageAggregator aggregator;
  Sample(aggregator, Time0, {CreateClient(1, 5000, 5000, 10), CreateClient(2, 5000, 5000, 10)});
  // A client was closed, so the sums are smaller than before
  Sample(aggregator, Time1, {CreateClient(1, 6000, 6000, 10)});

  float usage = 0.0f;
  EXPECT_FALSE(aggregator.TryGetUsagePercentage(usage));
}


TEST_F(Test_DrmClientUsageAggregator, NoTimeBetweenTheSamples_HasNoUsage)
{
  DrmClientUsageAggregator aggregator;
  Sample(aggregator, Time0, {CreateClient(1, 0, 0, 10)});
  Sample(aggregator, Time0, {CreateClient(1, 1000, 0, 10)});

  float usage = 0.0f;
  EXPECT_FALSE(aggregator.TryGetUsagePercentage(usage));
}
