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
#include <FslUtil/Vulkan1_0/UnitTest/Helper/Common.hpp>
#include <FslUtil/Vulkan1_0/UnitTest/Helper/TestFixtureFslUtil_Vulkan1_0.hpp>
#include <FslUtil/Vulkan1_0/Util/TimeDomainUtil.hpp>

using namespace Fsl;

namespace
{
  using TestFixtureFslUtil_Vulkan1_0_TimeDomainUtil = TestFixtureFslUtil_Vulkan1_0;
}

#ifdef FSL_VULKAN_CALIBRATED_TIMESTAMPS_SUPPORTED

using namespace Fsl::Vulkan;

TEST_F(TestFixtureFslUtil_Vulkan1_0_TimeDomainUtil, GetHostTimeDomain)
{
#ifdef _WIN32
  EXPECT_EQ(VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR, TimeDomainUtil::GetHostTimeDomain());
#else
  EXPECT_EQ(VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR, TimeDomainUtil::GetHostTimeDomain());
#endif
  EXPECT_TRUE(TimeDomainUtil::IsHostClock(TimeDomainUtil::GetHostTimeDomain()));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_TimeDomainUtil, IsHostClock)
{
  EXPECT_TRUE(TimeDomainUtil::IsHostClock(VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR));
  EXPECT_TRUE(TimeDomainUtil::IsHostClock(VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR));
  EXPECT_TRUE(TimeDomainUtil::IsHostClock(VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR));
  EXPECT_FALSE(TimeDomainUtil::IsHostClock(VK_TIME_DOMAIN_DEVICE_KHR));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_TimeDomainUtil, ToTickCount_PerformanceCounter)
{
  // A counter that counts in ticks
  EXPECT_EQ(TickCount(123456789), TimeDomainUtil::ToTickCount(VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR, 123456789u, 10000000u));
  // A counter that counts in microseconds
  EXPECT_EQ(TickCount(20000000), TimeDomainUtil::ToTickCount(VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR, 2000000u, 1000000u));
  // A counter that is faster than the ticks and not a whole number of counts per tick
  EXPECT_EQ(TickCount(10000000), TimeDomainUtil::ToTickCount(VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR, 24000000u, 24000000u));
  EXPECT_EQ(TickCount(5000000), TimeDomainUtil::ToTickCount(VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR, 12000000u, 24000000u));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_TimeDomainUtil, ToTickCount_PerformanceCounter_NoFrequency)
{
  EXPECT_EQ(TickCount(), TimeDomainUtil::ToTickCount(VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR, 123456789u, 0u));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_TimeDomainUtil, ToTickCount_ClockMonotonic)
{
  // Nanoseconds, the frequency is not used
  EXPECT_EQ(TickCount(10), TimeDomainUtil::ToTickCount(VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR, 1000u, 0u));
  EXPECT_EQ(TickCount(10), TimeDomainUtil::ToTickCount(VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR, 1099u, 12345u));
  EXPECT_EQ(TickCount(10000000), TimeDomainUtil::ToTickCount(VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR, 1000000000u, 0u));
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_TimeDomainUtil, ToTickCount_NotAHostClock)
{
  // The device clock can not be converted without a calibration
  EXPECT_EQ(TickCount(), TimeDomainUtil::ToTickCount(VK_TIME_DOMAIN_DEVICE_KHR, 123456789u, 10000000u));
}

#else

TEST_F(TestFixtureFslUtil_Vulkan1_0_TimeDomainUtil, NotSupportedByTheHeaders)
{
  std::cout << "\nSkipped 'TimeDomainUtil' (built with Vulkan headers that lack VK_KHR_calibrated_timestamps)\n";
}

#endif
