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
#include <FslBase/Time/NanosecondTimeSpan.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>

using namespace Fsl;

namespace
{
  using TestTime_NanosecondTimeSpan = TestFixtureFslBase;
}


TEST(TestTime_NanosecondTimeSpan, Construct_Default)
{
  const NanosecondTimeSpan value;
  EXPECT_EQ(0, value.TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Construct_Nanoseconds)
{
  const NanosecondTimeSpan value(10);
  EXPECT_EQ(10, value.TotalNanoseconds());
}

TEST(TestTime_NanosecondTimeSpan, Construct_DaysHoursMinutesSecondsMilliseconds)
{
  const NanosecondTimeSpan value(1, 2, 3, 4, 5);
  EXPECT_EQ((1 * NanosecondTimeSpan::NanosecondsPerDay) + (2 * NanosecondTimeSpan::NanosecondsPerHour) + (3 * NanosecondTimeSpan::NanosecondsPerMinute) + (4 * NanosecondTimeSpan::NanosecondsPerSecond) +
              (5 * NanosecondTimeSpan::NanosecondsPerMillisecond),
            value.TotalNanoseconds());
}

TEST(TestTime_NanosecondTimeSpan, Construct_DaysHoursMinutesSecondsMilliseconds_LargeMilliseconds)
{
  // 1000000ms * 1000000 nanoseconds per millisecond does not fit in a int32_t
  const NanosecondTimeSpan value(0, 0, 0, 0, 1000000);
  EXPECT_EQ(int64_t{1000000} * NanosecondTimeSpan::NanosecondsPerMillisecond, value.TotalNanoseconds());
  EXPECT_EQ(1000.0, value.TotalSeconds());
}

TEST(TestTime_NanosecondTimeSpan, Days)
{
  constexpr auto Units = 2;
  constexpr auto Nanoseconds = Units * NanosecondTimeSpan::NanosecondsPerDay;

  const NanosecondTimeSpan value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(Units, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
}


TEST(TestTime_NanosecondTimeSpan, Hours)
{
  constexpr auto Units = 2;
  constexpr auto Nanoseconds = Units * NanosecondTimeSpan::NanosecondsPerHour;

  const NanosecondTimeSpan value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(Units, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
}


TEST(TestTime_NanosecondTimeSpan, Minutes)
{
  constexpr auto Units = 2;
  constexpr auto Nanoseconds = Units * NanosecondTimeSpan::NanosecondsPerMinute;

  const NanosecondTimeSpan value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(Units, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
}


TEST(TestTime_NanosecondTimeSpan, Seconds)
{
  constexpr auto Units = 2;
  constexpr auto Nanoseconds = Units * NanosecondTimeSpan::NanosecondsPerSecond;

  const NanosecondTimeSpan value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(Units, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
}


TEST(TestTime_NanosecondTimeSpan, Milliseconds)
{
  constexpr auto Units = 2;
  constexpr auto Nanoseconds = Units * NanosecondTimeSpan::NanosecondsPerMillisecond;

  const NanosecondTimeSpan value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(Units, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
}


TEST(TestTime_NanosecondTimeSpan, Microseconds)
{
  constexpr auto Units = 2;
  constexpr auto Nanoseconds = Units * NanosecondTimeSpan::NanosecondsPerMicrosecond;

  const NanosecondTimeSpan value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(Units, value.Microseconds());
}


TEST(TestTime_NanosecondTimeSpan, Nanoseconds)
{
  constexpr auto Units = 2;
  constexpr auto Nanoseconds = Units;

  const NanosecondTimeSpan value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
  EXPECT_EQ(Units, value.Nanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, EveryPartOfAValue)
{
  // 1 day, 2 hours, 3 minutes, 4 seconds, 5 milliseconds, 6 microseconds and 7 nanoseconds
  const NanosecondTimeSpan value(NanosecondTimeSpan(1, 2, 3, 4, 5).TotalNanoseconds() + (6 * NanosecondTimeSpan::NanosecondsPerMicrosecond) + 7);
  EXPECT_EQ(1, value.Days());
  EXPECT_EQ(2, value.Hours());
  EXPECT_EQ(3, value.Minutes());
  EXPECT_EQ(4, value.Seconds());
  EXPECT_EQ(5, value.Milliseconds());
  EXPECT_EQ(6, value.Microseconds());
  EXPECT_EQ(7, value.Nanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Total)
{
  const NanosecondTimeSpan value(NanosecondTimeSpan::NanosecondsPerDay * 3);
  EXPECT_DOUBLE_EQ(3.0, value.TotalDays());
  EXPECT_DOUBLE_EQ(3.0 * 24.0, value.TotalHours());
  EXPECT_DOUBLE_EQ(3.0 * 24.0 * 60.0, value.TotalMinutes());
  EXPECT_DOUBLE_EQ(3.0 * 24.0 * 60.0 * 60.0, value.TotalSeconds());
  EXPECT_DOUBLE_EQ(3.0 * 24.0 * 60.0 * 60.0 * 1000.0, value.TotalMilliseconds());
  EXPECT_DOUBLE_EQ(3.0 * 24.0 * 60.0 * 60.0 * 1000.0 * 1000.0, value.TotalMicroseconds());
  // A value below the resolution of a TimeSpan
  EXPECT_DOUBLE_EQ(0.000000001, NanosecondTimeSpan(1).TotalSeconds());
}


TEST(TestTime_NanosecondTimeSpan, From)
{
  EXPECT_EQ(NanosecondTimeSpan::NanosecondsPerDay * 2, NanosecondTimeSpan::FromDays(2).TotalNanoseconds());
  EXPECT_EQ(NanosecondTimeSpan::NanosecondsPerHour * 2, NanosecondTimeSpan::FromHours(2).TotalNanoseconds());
  EXPECT_EQ(NanosecondTimeSpan::NanosecondsPerMinute * 2, NanosecondTimeSpan::FromMinutes(2).TotalNanoseconds());
  EXPECT_EQ(NanosecondTimeSpan::NanosecondsPerSecond * 2, NanosecondTimeSpan::FromSeconds(int32_t{2}).TotalNanoseconds());
  EXPECT_EQ(NanosecondTimeSpan::NanosecondsPerSecond * 2, NanosecondTimeSpan::FromSeconds(uint32_t{2}).TotalNanoseconds());
  EXPECT_EQ(NanosecondTimeSpan::NanosecondsPerSecond * 2, NanosecondTimeSpan::FromSeconds(int64_t{2}).TotalNanoseconds());
  EXPECT_EQ(NanosecondTimeSpan::NanosecondsPerSecond / 2, NanosecondTimeSpan::FromSeconds(0.5).TotalNanoseconds());
  EXPECT_EQ(int64_t{NanosecondTimeSpan::NanosecondsPerMillisecond} * 2, NanosecondTimeSpan::FromMilliseconds(2).TotalNanoseconds());
  EXPECT_EQ(int64_t{NanosecondTimeSpan::NanosecondsPerMicrosecond} * 2, NanosecondTimeSpan::FromMicroseconds(2).TotalNanoseconds());
  EXPECT_EQ(2, NanosecondTimeSpan::FromNanoseconds(2).TotalNanoseconds());
  EXPECT_EQ(-2, NanosecondTimeSpan::FromNanoseconds(-2).TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, From_OutOfRange)
{
  EXPECT_THROW(NanosecondTimeSpan::FromDays(NanosecondTimeSpan::MaxDays + 1), OverflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromDays(NanosecondTimeSpan::MinDays - 1), UnderflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromHours(NanosecondTimeSpan::MaxHours + 1), OverflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromHours(NanosecondTimeSpan::MinHours - 1), UnderflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromMinutes(NanosecondTimeSpan::MaxMinutes + 1), OverflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromMinutes(NanosecondTimeSpan::MinMinutes - 1), UnderflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromSeconds(NanosecondTimeSpan::MaxSeconds + 1), OverflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromSeconds(NanosecondTimeSpan::MinSeconds - 1), UnderflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromMilliseconds(NanosecondTimeSpan::MaxMilliseconds + 1), OverflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromMilliseconds(NanosecondTimeSpan::MinMilliseconds - 1), UnderflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromMicroseconds(NanosecondTimeSpan::MaxMicroseconds + 1), OverflowException);
  EXPECT_THROW(NanosecondTimeSpan::FromMicroseconds(NanosecondTimeSpan::MinMicroseconds - 1), UnderflowException);
  // The largest values are fine
  EXPECT_EQ(NanosecondTimeSpan::MaxDays, NanosecondTimeSpan::FromDays(NanosecondTimeSpan::MaxDays).Days());
  EXPECT_EQ(NanosecondTimeSpan::MaxSeconds * NanosecondTimeSpan::NanosecondsPerSecond,
            NanosecondTimeSpan::FromSeconds(NanosecondTimeSpan::MaxSeconds).TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_Negate)
{
  EXPECT_EQ(-7, (-NanosecondTimeSpan(7)).TotalNanoseconds());
  EXPECT_EQ(7, (-NanosecondTimeSpan(-7)).TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_AddEqual)
{
  constexpr auto Nanoseconds0 = 7;
  constexpr auto Nanoseconds1 = 21;
  NanosecondTimeSpan value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  value0 += value1;

  EXPECT_EQ(Nanoseconds0 + Nanoseconds1, value0.TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_SubEqual)
{
  constexpr auto Nanoseconds0 = 7;
  constexpr auto Nanoseconds1 = 21;
  NanosecondTimeSpan value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  value0 -= value1;

  EXPECT_EQ(Nanoseconds0 - Nanoseconds1, value0.TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_MulEqual)
{
  constexpr auto Nanoseconds0 = 7;
  constexpr int32_t Value = 2;
  NanosecondTimeSpan value0(Nanoseconds0);

  value0 *= Value;

  EXPECT_EQ(Nanoseconds0 * Value, value0.TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_DivEqual)
{
  constexpr auto Nanoseconds0 = 8;
  constexpr int32_t Value = 2;
  NanosecondTimeSpan value0(Nanoseconds0);

  value0 /= Value;

  EXPECT_EQ(Nanoseconds0 / Value, value0.TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_Add)
{
  constexpr auto Nanoseconds0 = 7;
  constexpr auto Nanoseconds1 = 21;
  const NanosecondTimeSpan value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  EXPECT_EQ(Nanoseconds0 + Nanoseconds1, (value0 + value1).TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_Sub)
{
  constexpr auto Nanoseconds0 = 7;
  constexpr auto Nanoseconds1 = 21;
  const NanosecondTimeSpan value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  EXPECT_EQ(Nanoseconds0 - Nanoseconds1, (value0 - value1).TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_MulNanosecondTimeSpanVal)
{
  constexpr auto Nanoseconds0 = 7;
  constexpr int32_t Value = 2;
  const NanosecondTimeSpan value0(Nanoseconds0);

  EXPECT_EQ(Nanoseconds0 * Value, (value0 * Value).TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_MulValNanosecondTimeSpan)
{
  constexpr auto Nanoseconds0 = 7;
  constexpr int32_t Value = 2;
  const NanosecondTimeSpan value0(Nanoseconds0);

  EXPECT_EQ(Value * Nanoseconds0, (Value * value0).TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_DivNanosecondTimeSpanVal)
{
  constexpr auto Nanoseconds0 = 8;
  constexpr int32_t Value = 2;
  const NanosecondTimeSpan value0(Nanoseconds0);

  EXPECT_EQ(Nanoseconds0 / Value, (value0 / Value).TotalNanoseconds());
}


TEST(TestTime_NanosecondTimeSpan, Op_Equal)
{
  EXPECT_TRUE(NanosecondTimeSpan(7) == NanosecondTimeSpan(7));
}

TEST(TestTime_NanosecondTimeSpan, Op_NotEqual)
{
  EXPECT_TRUE(NanosecondTimeSpan(7) != NanosecondTimeSpan(8));
}

TEST(TestTime_NanosecondTimeSpan, Op_Less)
{
  EXPECT_TRUE(NanosecondTimeSpan(7) < NanosecondTimeSpan(8));
  EXPECT_FALSE(NanosecondTimeSpan(8) < NanosecondTimeSpan(8));
  EXPECT_FALSE(NanosecondTimeSpan(9) < NanosecondTimeSpan(8));
}

TEST(TestTime_NanosecondTimeSpan, Op_LessOrEqual)
{
  EXPECT_TRUE(NanosecondTimeSpan(7) <= NanosecondTimeSpan(8));
  EXPECT_TRUE(NanosecondTimeSpan(8) <= NanosecondTimeSpan(8));
  EXPECT_FALSE(NanosecondTimeSpan(9) <= NanosecondTimeSpan(8));
}

TEST(TestTime_NanosecondTimeSpan, Op_Greater)
{
  EXPECT_FALSE(NanosecondTimeSpan(7) > NanosecondTimeSpan(8));
  EXPECT_FALSE(NanosecondTimeSpan(8) > NanosecondTimeSpan(8));
  EXPECT_TRUE(NanosecondTimeSpan(9) > NanosecondTimeSpan(8));
}

TEST(TestTime_NanosecondTimeSpan, Op_GreaterOrEqual)
{
  EXPECT_FALSE(NanosecondTimeSpan(7) >= NanosecondTimeSpan(8));
  EXPECT_TRUE(NanosecondTimeSpan(8) >= NanosecondTimeSpan(8));
  EXPECT_TRUE(NanosecondTimeSpan(9) >= NanosecondTimeSpan(8));
}
