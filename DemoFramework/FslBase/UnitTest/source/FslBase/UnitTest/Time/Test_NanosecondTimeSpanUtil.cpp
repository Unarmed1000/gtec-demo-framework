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



#include <FslBase/Exceptions.hpp>
#include <FslBase/Time/NanosecondTimeSpanUtil.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <limits>

using namespace Fsl;

namespace
{
  using TestTime_NanosecondTimeSpanUtil = TestFixtureFslBase;
}


TEST(TestTime_NanosecondTimeSpanUtil, FromFrequencyRational)
{
  EXPECT_EQ(16666667, NanosecondTimeSpanUtil::FromFrequencyRational(60, 1).TotalNanoseconds());
  EXPECT_EQ(8333333, NanosecondTimeSpanUtil::FromFrequencyRational(120, 1).TotalNanoseconds());
  // 59.94 Hz
  EXPECT_EQ(16683333, NanosecondTimeSpanUtil::FromFrequencyRational(60000, 1001).TotalNanoseconds());
  // A mode of 240.016 Hz: 4166388.907 nanoseconds, which is 41663.889 ticks of 100 nanoseconds
  EXPECT_EQ(4166389, NanosecondTimeSpanUtil::FromFrequencyRational(240016, 1000).TotalNanoseconds());
  EXPECT_EQ(1000000000, NanosecondTimeSpanUtil::FromFrequencyRational(1, 1).TotalNanoseconds());
  // Rounded to the nearest nanosecond: 1/3 Hz is 3333333333.33 ns, 3 Hz is 333333333.33 ns and 7 Hz is 142857142.857 ns
  EXPECT_EQ(3000000000, NanosecondTimeSpanUtil::FromFrequencyRational(1, 3).TotalNanoseconds());
  EXPECT_EQ(333333333, NanosecondTimeSpanUtil::FromFrequencyRational(3, 1).TotalNanoseconds());
  EXPECT_EQ(142857143, NanosecondTimeSpanUtil::FromFrequencyRational(7, 1).TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpanUtil, FromFrequencyRational_Invalid)
{
  EXPECT_EQ(NanosecondTimeSpan(), NanosecondTimeSpanUtil::FromFrequencyRational(0, 1));
  EXPECT_EQ(NanosecondTimeSpan(), NanosecondTimeSpanUtil::FromFrequencyRational(1, 0));
  EXPECT_EQ(NanosecondTimeSpan(), NanosecondTimeSpanUtil::FromFrequencyRational(0, 0));
  // The denominator times the nanoseconds of a second does not fit
  EXPECT_EQ(NanosecondTimeSpan(), NanosecondTimeSpanUtil::FromFrequencyRational(1, std::numeric_limits<uint64_t>::max()));
  // The period does not fit: more than the range of the type
  EXPECT_EQ(NanosecondTimeSpan(), NanosecondTimeSpanUtil::FromFrequencyRational(1, 10000000000));
}


TEST(TestTime_NanosecondTimeSpanUtil, ToFrequencyHz)
{
  EXPECT_DOUBLE_EQ(1.0, NanosecondTimeSpanUtil::ToFrequencyHz(NanosecondTimeSpan(1000000000)));
  EXPECT_DOUBLE_EQ(250.0, NanosecondTimeSpanUtil::ToFrequencyHz(NanosecondTimeSpan(4000000)));
  EXPECT_NEAR(240.016, NanosecondTimeSpanUtil::ToFrequencyHz(NanosecondTimeSpan(4166389)), 0.00001);
  EXPECT_EQ(0.0, NanosecondTimeSpanUtil::ToFrequencyHz(NanosecondTimeSpan()));
  EXPECT_EQ(0.0, NanosecondTimeSpanUtil::ToFrequencyHz(NanosecondTimeSpan(-4000000)));
}


TEST(TestTime_NanosecondTimeSpanUtil, ToTimeSpan_RoundsToTheNearestTick)
{
  EXPECT_EQ(0, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan()).Ticks());
  EXPECT_EQ(1, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(100)).Ticks());
  EXPECT_EQ(0, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(49)).Ticks());
  EXPECT_EQ(1, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(50)).Ticks());
  EXPECT_EQ(1, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(149)).Ticks());
  EXPECT_EQ(2, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(150)).Ticks());
  EXPECT_EQ(0, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(-49)).Ticks());
  EXPECT_EQ(-1, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(-50)).Ticks());
  EXPECT_EQ(-2, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(-150)).Ticks());
  // The refresh period of a mode of 240.016 Hz
  EXPECT_EQ(41664, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(4166389)).Ticks());
  // The ends of the range
  EXPECT_EQ(92233720368547758, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(std::numeric_limits<int64_t>::max())).Ticks());
  EXPECT_EQ(-92233720368547758, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpan(std::numeric_limits<int64_t>::min())).Ticks());
}


TEST(TestTime_NanosecondTimeSpanUtil, FromTimeSpan)
{
  EXPECT_EQ(0, NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan()).TotalNanoseconds());
  EXPECT_EQ(100, NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan(1)).TotalNanoseconds());
  EXPECT_EQ(-100, NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan(-1)).TotalNanoseconds());
  EXPECT_EQ(4166400, NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan(41664)).TotalNanoseconds());
  EXPECT_EQ(NanosecondTimeSpan::NanosecondsPerDay, NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan(TimeSpan::TicksPerDay)).TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpanUtil, FromTimeSpan_OutOfRange)
{
  constexpr int64_t MaxTicks = std::numeric_limits<int64_t>::max() / 100;
  constexpr int64_t MinTicks = std::numeric_limits<int64_t>::min() / 100;
  EXPECT_EQ(MaxTicks * 100, NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan(MaxTicks)).TotalNanoseconds());
  EXPECT_EQ(MinTicks * 100, NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan(MinTicks)).TotalNanoseconds());
  EXPECT_THROW(NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan(MaxTicks + 1)), OverflowException);
  EXPECT_THROW(NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan(MinTicks - 1)), UnderflowException);
}


TEST(TestTime_NanosecondTimeSpanUtil, ToTimeSpan_FromTimeSpan_RoundTrip)
{
  for (const int64_t ticks : {int64_t{0}, int64_t{1}, int64_t{-1}, int64_t{41664}, TimeSpan::TicksPerDay, -TimeSpan::TicksPerHour})
  {
    EXPECT_EQ(ticks, NanosecondTimeSpanUtil::ToTimeSpan(NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan(ticks))).Ticks());
  }
}
