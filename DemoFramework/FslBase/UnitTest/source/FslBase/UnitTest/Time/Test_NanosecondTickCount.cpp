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

#include <FslBase/NumericCast.hpp>
#include <FslBase/Time/NanosecondTickCount.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>

using namespace Fsl;

namespace
{
  using TestTime_NanosecondTickCount = TestFixtureFslBase;
}


TEST(TestTime_NanosecondTickCount, Construct_Default)
{
  const NanosecondTickCount value;
  EXPECT_EQ(0u, value.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Construct_Nanoseconds)
{
  const NanosecondTickCount value(10);
  EXPECT_EQ(10u, value.TotalNanoseconds());
}

TEST(TestTime_NanosecondTickCount, Construct_Limits)
{
  const NanosecondTickCount minValue(std::numeric_limits<int64_t>::min());
  const NanosecondTickCount maxValue(std::numeric_limits<int64_t>::max());
  const NanosecondTickCount negativeValue(-1);
  const NanosecondTickCount zeroValue(0);
  const NanosecondTickCount plusValue(1);
  EXPECT_EQ(minValue.TotalNanoseconds(), std::numeric_limits<int64_t>::min());
  EXPECT_EQ(maxValue.TotalNanoseconds(), std::numeric_limits<int64_t>::max());
  EXPECT_EQ(negativeValue.TotalNanoseconds(), -1);
  EXPECT_EQ(zeroValue.TotalNanoseconds(), 0);
  EXPECT_EQ(plusValue.TotalNanoseconds(), 1);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, Days)
{
  constexpr int64_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerDay;

  const NanosecondTickCount value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(Units, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
}


TEST(TestTime_NanosecondTickCount, Hours)
{
  constexpr int64_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerHour;

  const NanosecondTickCount value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(Units, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
}


TEST(TestTime_NanosecondTickCount, Minutes)
{
  constexpr int64_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMinute;

  const NanosecondTickCount value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(Units, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
}


TEST(TestTime_NanosecondTickCount, Seconds)
{
  constexpr int64_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerSecond;

  const NanosecondTickCount value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(Units, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
}


TEST(TestTime_NanosecondTickCount, Milliseconds)
{
  constexpr int64_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMillisecond;

  const NanosecondTickCount value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(Units, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
}


TEST(TestTime_NanosecondTickCount, Microseconds)
{
  constexpr int64_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMicrosecond;

  const NanosecondTickCount value(Nanoseconds);
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(Units, value.Microseconds());
}


// ---------------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, FromDays_Signed)
{
  constexpr int32_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerDay;
  const auto value = NanosecondTickCount::FromDays(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(Units, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());


  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalDays());
}

TEST(TestTime_NanosecondTickCount, FromDays_Signed_MinValue)
{
  constexpr auto Units = NumericCast<int32_t>(std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerDay);
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerDay;
  const auto value = NanosecondTickCount::FromDays(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(Units, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());


  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalDays());
}

TEST(TestTime_NanosecondTickCount, FromDays_Signed_MaxValue)
{
  constexpr auto Units = NumericCast<int32_t>(std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerDay);
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerDay;
  const auto value = NanosecondTickCount::FromDays(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(Units, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalDays());
}

TEST(TestTime_NanosecondTickCount, FromDays_Signed_Underflow)
{
  constexpr auto Units = NumericCast<int32_t>((std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerDay) - 1);
  EXPECT_THROW(NanosecondTickCount::FromDays(Units), UnderflowException);
}


TEST(TestTime_NanosecondTickCount, FromDays_Signed_Overflow)
{
  constexpr auto Units = NumericCast<int32_t>((std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerDay) + 1);
  EXPECT_THROW(NanosecondTickCount::FromDays(Units), OverflowException);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, FromHours_Signed)
{
  constexpr int32_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerHour;
  const auto value = NanosecondTickCount::FromHours(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 24), value.Days());
  EXPECT_EQ(Units % 24, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());


  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalHours());
}


TEST(TestTime_NanosecondTickCount, FromHours_Signed_MinValue)
{
  constexpr auto Units = NumericCast<int32_t>(std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerHour);
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerHour;
  const auto value = NanosecondTickCount::FromHours(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 24), value.Days());
  EXPECT_EQ(Units % 24, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalHours());
}

TEST(TestTime_NanosecondTickCount, FromHours_Signed_MaxValue)
{
  constexpr auto Units = NumericCast<int32_t>(std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerHour);
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerHour;
  const auto value = NanosecondTickCount::FromHours(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 24), value.Days());
  EXPECT_EQ(Units % 24, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalHours());
}


TEST(TestTime_NanosecondTickCount, FromHours_Signed_Underflow)
{
  constexpr auto Units = NumericCast<int32_t>((std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerHour) - 1);
  EXPECT_THROW(NanosecondTickCount::FromDays(Units), UnderflowException);
}


TEST(TestTime_NanosecondTickCount, FromHours_Signed_Overflow)
{
  constexpr auto Units = NumericCast<int32_t>((std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerHour) + 1);
  EXPECT_THROW(NanosecondTickCount::FromHours(Units), OverflowException);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, FromMinutes_Signed)
{
  constexpr int64_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMinute;
  const auto value = NanosecondTickCount::FromMinutes(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 60 / 24), value.Days());
  EXPECT_EQ((Units / 60) % 24, value.Hours());
  EXPECT_EQ(Units % 60, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalMinutes());
}


TEST(TestTime_NanosecondTickCount, FromMinutes_Signed_MinValue)
{
  constexpr int64_t Units = (std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerMinute);
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMinute;
  const auto value = NanosecondTickCount::FromMinutes(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 60 / 24), value.Days());
  EXPECT_EQ((Units / 60) % 24, value.Hours());
  EXPECT_EQ(Units % 60, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalMinutes());
}

TEST(TestTime_NanosecondTickCount, FromMinutes_Signed_MaxValue)
{
  constexpr int64_t Units = std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerMinute;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMinute;
  const auto value = NanosecondTickCount::FromMinutes(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 60 / 24), value.Days());
  EXPECT_EQ((Units / 60) % 24, value.Hours());
  EXPECT_EQ(Units % 60, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalMinutes());
}

TEST(TestTime_NanosecondTickCount, FromMinutes_Signed_Underflow)
{
  constexpr int64_t Units = (std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerMinute) - 1;
  EXPECT_THROW(NanosecondTickCount::FromMinutes(Units), UnderflowException);
}


TEST(TestTime_NanosecondTickCount, FromMinutes_Signed_Overflow)
{
  constexpr int64_t Units = (std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerMinute) + 1;
  EXPECT_THROW(NanosecondTickCount::FromMinutes(Units), OverflowException);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, FromSeconds_Signed)
{
  constexpr int64_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerSecond;
  const auto value = NanosecondTickCount::FromSeconds(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 60) % 60, value.Minutes());
  EXPECT_EQ(Units % 60, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalSeconds());
}


TEST(TestTime_NanosecondTickCount, FromSeconds_Signed_MinValue)
{
  constexpr int64_t Units = (std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerSecond);
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerSecond;
  const auto value = NanosecondTickCount::FromSeconds(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 60) % 60, value.Minutes());
  EXPECT_EQ(Units % 60, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalSeconds());
}

TEST(TestTime_NanosecondTickCount, FromSeconds_Signed_MaxValue)
{
  constexpr int64_t Units = std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerSecond;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerSecond;
  const auto value = NanosecondTickCount::FromSeconds(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 60) % 60, value.Minutes());
  EXPECT_EQ(Units % 60, value.Seconds());
  EXPECT_EQ(0, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalSeconds());
}

TEST(TestTime_NanosecondTickCount, FromSeconds_Signed_Underflow)
{
  constexpr int64_t Units = (std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerSecond) - 1;
  EXPECT_THROW(NanosecondTickCount::FromSeconds(Units), UnderflowException);
}


TEST(TestTime_NanosecondTickCount, FromSeconds_Signed_Overflow)
{
  constexpr int64_t Units = (std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerSecond) + 1;
  EXPECT_THROW(NanosecondTickCount::FromSeconds(Units), OverflowException);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, FromMilliseconds_Signed)
{
  constexpr int64_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMillisecond;
  const auto value = NanosecondTickCount::FromMilliseconds(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 1000 / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 1000 / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 1000 / 60) % 60, value.Minutes());
  EXPECT_EQ((Units / 1000) % 60, value.Seconds());
  EXPECT_EQ(Units % 1000, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());


  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalMilliseconds());
}


TEST(TestTime_NanosecondTickCount, FromMilliseconds_Signed_MinValue)
{
  constexpr int64_t Units = (std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerMillisecond);
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMillisecond;
  const auto value = NanosecondTickCount::FromMilliseconds(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 1000 / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 1000 / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 1000 / 60) % 60, value.Minutes());
  EXPECT_EQ((Units / 1000) % 60, value.Seconds());
  EXPECT_EQ(Units % 1000, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalMilliseconds());
}

TEST(TestTime_NanosecondTickCount, FromMilliseconds_Signed_MaxValue)
{
  constexpr int64_t Units = std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerMillisecond;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMillisecond;
  const auto value = NanosecondTickCount::FromMilliseconds(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 1000 / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 1000 / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 1000 / 60) % 60, value.Minutes());
  EXPECT_EQ((Units / 1000) % 60, value.Seconds());
  EXPECT_EQ(Units % 1000, value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalMilliseconds());
}

// overflow and underflow can not occur in FromMilliseconds


// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, FromMicroseconds_Signed)
{
  constexpr int64_t Units = 2;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMicrosecond;
  const auto value = NanosecondTickCount::FromMicroseconds(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 1000 / 1000 / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 1000 / 1000 / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 1000 / 1000 / 60) % 60, value.Minutes());
  EXPECT_EQ((Units / 1000 / 1000) % 60, value.Seconds());
  EXPECT_EQ((Units / 1000) % 1000, value.Milliseconds());
  EXPECT_EQ(Units % 1000, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalMicroseconds());
}


TEST(TestTime_NanosecondTickCount, FromMicroseconds_Signed_MinValue)
{
  constexpr int64_t Units = std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerMicrosecond;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMicrosecond;
  const auto value = NanosecondTickCount::FromMicroseconds(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 1000 / 1000 / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 1000 / 1000 / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 1000 / 1000 / 60) % 60, value.Minutes());
  EXPECT_EQ((Units / 1000 / 1000) % 60, value.Seconds());
  EXPECT_EQ((Units / 1000) % 1000, value.Milliseconds());
  EXPECT_EQ(Units % 1000, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalMicroseconds());
}

TEST(TestTime_NanosecondTickCount, FromMicroseconds_Signed_MaxValue)
{
  constexpr int64_t Units = std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerMicrosecond;
  constexpr int64_t Nanoseconds = Units * NanosecondTickCount::NanosecondsPerMicrosecond;
  const auto value = NanosecondTickCount::FromMicroseconds(Units);

  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ((Units / 1000 / 1000 / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 1000 / 1000 / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 1000 / 1000 / 60) % 60, value.Minutes());
  EXPECT_EQ((Units / 1000 / 1000) % 60, value.Seconds());
  EXPECT_EQ((Units / 1000) % 1000, value.Milliseconds());
  EXPECT_EQ(Units % 1000, value.Microseconds());

  EXPECT_DOUBLE_EQ(static_cast<double>(Units), value.TotalMicroseconds());
}

// overflow and underflow can not occur in FromMicroseconds

// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, FromNanoseconds_Signed)
{
  constexpr int64_t Milliseconds = 2;
  constexpr int64_t Nanoseconds = (Milliseconds * NanosecondTickCount::NanosecondsPerMillisecond) + 7;
  const auto value = NanosecondTickCount::FromNanoseconds(Nanoseconds);

  // Nothing is lost
  EXPECT_EQ(Nanoseconds, value.TotalNanoseconds());
  EXPECT_EQ(0, value.Days());
  EXPECT_EQ(0, value.Hours());
  EXPECT_EQ(0, value.Minutes());
  EXPECT_EQ(0, value.Seconds());
  EXPECT_EQ(NumericCast<int32_t>(Milliseconds), value.Milliseconds());
  EXPECT_EQ(0, value.Microseconds());
  EXPECT_EQ(7, value.Nanoseconds());
}

TEST(TestTime_NanosecondTickCount, FromNanoseconds_Signed_MinValue)
{
  constexpr int64_t Units = std::numeric_limits<int64_t>::min();
  const auto value = NanosecondTickCount::FromNanoseconds(Units);

  EXPECT_EQ(Units, value.TotalNanoseconds());
  EXPECT_EQ((Units / 1000 / 1000 / 1000 / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 1000 / 1000 / 1000 / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 1000 / 1000 / 1000 / 60) % 60, value.Minutes());
  EXPECT_EQ((Units / 1000 / 1000 / 1000) % 60, value.Seconds());
  EXPECT_EQ((Units / 1000 / 1000) % 1000, value.Milliseconds());
  EXPECT_EQ((Units / 1000) % 1000, value.Microseconds());
  EXPECT_EQ(Units % 1000, value.Nanoseconds());
}

TEST(TestTime_NanosecondTickCount, FromNanoseconds_Signed_MaxValue)
{
  constexpr int64_t Units = std::numeric_limits<int64_t>::max();
  const auto value = NanosecondTickCount::FromNanoseconds(Units);

  EXPECT_EQ(Units, value.TotalNanoseconds());
  EXPECT_EQ((Units / 1000 / 1000 / 1000 / 60 / 60 / 24), value.Days());
  EXPECT_EQ((Units / 1000 / 1000 / 1000 / 60 / 60) % 24, value.Hours());
  EXPECT_EQ((Units / 1000 / 1000 / 1000 / 60) % 60, value.Minutes());
  EXPECT_EQ((Units / 1000 / 1000 / 1000) % 60, value.Seconds());
  EXPECT_EQ((Units / 1000 / 1000) % 1000, value.Milliseconds());
  EXPECT_EQ((Units / 1000) % 1000, value.Microseconds());
  EXPECT_EQ(Units % 1000, value.Nanoseconds());
}

// overflow and underflow can not occur in FromNanoseconds

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, Op_AddEqual_NanosecondTickCount_NanosecondTimeSpan)
{
  constexpr int64_t Nanoseconds0 = 7;
  constexpr int64_t Nanoseconds1 = 21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  NanosecondTickCount result(value0);
  result += value1;

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) + static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

TEST(TestTime_NanosecondTickCount, Op_AddEqual_NanosecondTickCount_NanosecondTimeSpan_Underflow1)
{
  constexpr int64_t Nanoseconds0 = std::numeric_limits<int64_t>::min();
  constexpr int64_t Nanoseconds1 = -21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  NanosecondTickCount result(value0);
  result += value1;

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) + static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

TEST(TestTime_NanosecondTickCount, Op_AddEqual_NanosecondTickCount_NanosecondTimeSpan_Underflow2)
{
  constexpr int64_t Nanoseconds0 = -21;
  constexpr int64_t Nanoseconds1 = std::numeric_limits<int64_t>::min();
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  NanosecondTickCount result(value0);
  result += value1;

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) + static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

TEST(TestTime_NanosecondTickCount, Op_AddEqual_NanosecondTickCount_NanosecondTimeSpan_Overflow1)
{
  constexpr int64_t Nanoseconds0 = std::numeric_limits<int64_t>::max();
  constexpr int64_t Nanoseconds1 = 21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  NanosecondTickCount result(value0);
  result += value1;

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) + static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

TEST(TestTime_NanosecondTickCount, Op_AddEqual_NanosecondTickCount_NanosecondTimeSpan_Overflow2)
{
  constexpr int64_t Nanoseconds0 = 7;
  constexpr int64_t Nanoseconds1 = std::numeric_limits<int64_t>::max();
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  NanosecondTickCount result(value0);
  result += value1;

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) + static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, Op_SubEqual_NanosecondTickCount_NanosecondTimeSpan)
{
  constexpr int64_t Nanoseconds0 = 7;
  constexpr int64_t Nanoseconds1 = 21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  NanosecondTickCount result(value0);
  result -= value1;
  const NanosecondTimeSpan diff = value0 - result;

  EXPECT_EQ(diff, value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_SubEqual_NanosecondTickCount_NanosecondTimeSpan_Underflow1)
{
  constexpr int64_t Nanoseconds0 = std::numeric_limits<int64_t>::min();
  constexpr int64_t Nanoseconds1 = 21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  NanosecondTickCount result(value0);
  result -= value1;
  const NanosecondTimeSpan diff = value0 - result;

  EXPECT_EQ(diff, value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_SubEqual_NanosecondTickCount_NanosecondTimeSpan_Underflow2)
{
  constexpr int64_t Nanoseconds0 = -3;
  constexpr int64_t Nanoseconds1 = std::numeric_limits<int64_t>::max();
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  NanosecondTickCount result(value0);
  result -= value1;
  const NanosecondTimeSpan diff = value0 - result;

  EXPECT_EQ(diff, value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_SubEqual_NanosecondTickCount_NanosecondTimeSpan_Overflow1)
{
  constexpr int64_t Nanoseconds0 = std::numeric_limits<int64_t>::max();
  constexpr int64_t Nanoseconds1 = -21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  NanosecondTickCount result(value0);
  result -= value1;
  const NanosecondTimeSpan diff = value0 - result;

  EXPECT_EQ(diff, value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_SubEqual_NanosecondTickCount_NanosecondTimeSpan_Overflow2)
{
  constexpr int64_t Nanoseconds0 = 7;
  constexpr int64_t Nanoseconds1 = std::numeric_limits<int64_t>::min();
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  NanosecondTickCount result(value0);
  result -= value1;
  const NanosecondTimeSpan diff = value0 - result;

  EXPECT_EQ(diff, value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, Op_Equal)
{
  EXPECT_TRUE(NanosecondTickCount(7) == NanosecondTickCount(7));
}

TEST(TestTime_NanosecondTickCount, Op_NotEqual)
{
  EXPECT_TRUE(NanosecondTickCount(7) != NanosecondTickCount(8));
}

TEST(TestTime_NanosecondTickCount, Op_Less)
{
  EXPECT_TRUE(NanosecondTickCount(7) < NanosecondTickCount(8));
  EXPECT_FALSE(NanosecondTickCount(8) < NanosecondTickCount(8));
  EXPECT_FALSE(NanosecondTickCount(9) < NanosecondTickCount(8));
}

TEST(TestTime_NanosecondTickCount, Op_LessOrEqual)
{
  EXPECT_TRUE(NanosecondTickCount(7) <= NanosecondTickCount(8));
  EXPECT_TRUE(NanosecondTickCount(8) <= NanosecondTickCount(8));
  EXPECT_FALSE(NanosecondTickCount(9) <= NanosecondTickCount(8));
}

TEST(TestTime_NanosecondTickCount, Op_Greater)
{
  EXPECT_FALSE(NanosecondTickCount(7) > NanosecondTickCount(8));
  EXPECT_FALSE(NanosecondTickCount(8) > NanosecondTickCount(8));
  EXPECT_TRUE(NanosecondTickCount(9) > NanosecondTickCount(8));
}

TEST(TestTime_NanosecondTickCount, Op_GreaterOrEqual)
{
  EXPECT_FALSE(NanosecondTickCount(7) >= NanosecondTickCount(8));
  EXPECT_TRUE(NanosecondTickCount(8) >= NanosecondTickCount(8));
  EXPECT_TRUE(NanosecondTickCount(9) >= NanosecondTickCount(8));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, Op_Add_NanosecondTickCount_NanosecondTimeSpan)
{
  constexpr int64_t Nanoseconds0 = 7;
  constexpr int64_t Nanoseconds1 = 21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  const NanosecondTickCount result(value0 + value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) + static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

TEST(TestTime_NanosecondTickCount, Op_Add_NanosecondTickCount_NanosecondTimeSpan_Underflow1)
{
  constexpr int64_t Nanoseconds0 = std::numeric_limits<int64_t>::min();
  constexpr int64_t Nanoseconds1 = -21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  const NanosecondTickCount result(value0 + value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) + static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

TEST(TestTime_NanosecondTickCount, Op_Add_NanosecondTickCount_NanosecondTimeSpan_Underflow2)
{
  constexpr int64_t Nanoseconds0 = -21;
  constexpr int64_t Nanoseconds1 = std::numeric_limits<int64_t>::min();
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  const NanosecondTickCount result(value0 + value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) + static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

TEST(TestTime_NanosecondTickCount, Op_Add_NanosecondTickCount_NanosecondTimeSpan_Overflow1)
{
  constexpr int64_t Nanoseconds0 = std::numeric_limits<int64_t>::max();
  constexpr int64_t Nanoseconds1 = 21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  const NanosecondTickCount result(value0 + value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) + static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

TEST(TestTime_NanosecondTickCount, Op_Add_NanosecondTickCount_NanosecondTimeSpan_Overflow2)
{
  constexpr int64_t Nanoseconds0 = 7;
  constexpr int64_t Nanoseconds1 = std::numeric_limits<int64_t>::max();
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  const NanosecondTickCount result(value0 + value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) + static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, Op_Sub_NanosecondTickCount_NanosecondTickCount)
{
  constexpr int64_t Nanoseconds0 = 7;
  constexpr int64_t Nanoseconds1 = 21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTickCount value1(Nanoseconds1);

  const NanosecondTimeSpan result(value0 - value1);
  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_Sub_NanosecondTickCount_NanosecondTickCount_Underflow1)
{
  constexpr int64_t Nanoseconds0 = std::numeric_limits<int64_t>::min();
  constexpr int64_t Nanoseconds1 = 21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTickCount value1(Nanoseconds1);

  const NanosecondTimeSpan result(value0 - value1);
  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_Sub_NanosecondTickCount_NanosecondTickCount_Underflow2)
{
  constexpr int64_t Nanoseconds0 = -3;
  constexpr int64_t Nanoseconds1 = std::numeric_limits<int64_t>::max();
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTickCount value1(Nanoseconds1);

  const NanosecondTimeSpan result(value0 - value1);
  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_Sub_NanosecondTickCount_NanosecondTickCount_Overflow1)
{
  constexpr int64_t Nanoseconds0 = std::numeric_limits<int64_t>::max();
  constexpr int64_t Nanoseconds1 = -21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTickCount value1(Nanoseconds1);

  const NanosecondTimeSpan result(value0 - value1);
  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_Sub_NanosecondTickCount_NanosecondTickCount_Overflow2)
{
  constexpr int64_t Nanoseconds0 = 7;
  constexpr int64_t Nanoseconds1 = std::numeric_limits<int64_t>::min();
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTickCount value1(Nanoseconds1);

  const NanosecondTimeSpan result(value0 - value1);
  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

TEST(TestTime_NanosecondTickCount, Op_Sub_NanosecondTickCount_NanosecondTimeSpan)
{
  constexpr int64_t Nanoseconds0 = 7;
  constexpr int64_t Nanoseconds1 = 21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  const NanosecondTickCount result(value0 - value1);
  const NanosecondTimeSpan diff = value0 - result;

  EXPECT_EQ(diff, value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_Sub_NanosecondTickCount_NanosecondTimeSpan_Underflow1)
{
  constexpr int64_t Nanoseconds0 = std::numeric_limits<int64_t>::min();
  constexpr int64_t Nanoseconds1 = 21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  const NanosecondTickCount result(value0 - value1);
  const NanosecondTimeSpan diff = value0 - result;

  EXPECT_EQ(diff, value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_Sub_NanosecondTickCount_NanosecondTimeSpan_Underflow2)
{
  constexpr int64_t Nanoseconds0 = -3;
  constexpr int64_t Nanoseconds1 = std::numeric_limits<int64_t>::max();
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  const NanosecondTickCount result(value0 - value1);
  const NanosecondTimeSpan diff = value0 - result;

  EXPECT_EQ(diff, value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_Sub_NanosecondTickCount_NanosecondTimeSpan_Overflow1)
{
  constexpr int64_t Nanoseconds0 = std::numeric_limits<int64_t>::max();
  constexpr int64_t Nanoseconds1 = -21;
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  const NanosecondTickCount result(value0 - value1);
  const NanosecondTimeSpan diff = value0 - result;

  EXPECT_EQ(diff, value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCount, Op_Sub_NanosecondTickCount_NanosecondTimeSpan_Overflow2)
{
  constexpr int64_t Nanoseconds0 = 7;
  constexpr int64_t Nanoseconds1 = std::numeric_limits<int64_t>::min();
  const NanosecondTickCount value0(Nanoseconds0);
  const NanosecondTimeSpan value1(Nanoseconds1);

  const NanosecondTickCount result(value0 - value1);
  const NanosecondTimeSpan diff = value0 - result;

  EXPECT_EQ(diff, value1);

  const uint64_t expectedUnsignedResult = static_cast<uint64_t>(Nanoseconds0) - static_cast<uint64_t>(Nanoseconds1);
  const auto expectedResult = static_cast<int64_t>(expectedUnsignedResult);

  EXPECT_EQ(expectedResult, result.TotalNanoseconds());
}
