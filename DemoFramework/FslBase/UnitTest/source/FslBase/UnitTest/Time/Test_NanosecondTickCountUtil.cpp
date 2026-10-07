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
#include <FslBase/Time/NanosecondTickCountUtil.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <limits>

using namespace Fsl;

namespace
{
  using TestTime_NanosecondTickCountUtil = TestFixtureFslBase;
}


TEST(TestTime_NanosecondTickCountUtil, ToTickCount_TheTickThePointInTimeLiesIn)
{
  EXPECT_EQ(0, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount()).Ticks());
  EXPECT_EQ(0, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount(99)).Ticks());
  EXPECT_EQ(1, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount(100)).Ticks());
  EXPECT_EQ(1, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount(199)).Ticks());
  EXPECT_EQ(2, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount(200)).Ticks());
  EXPECT_EQ(41663, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount(4166389)).Ticks());
  // Before zero the tick is the one below as well
  EXPECT_EQ(-1, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount(-1)).Ticks());
  EXPECT_EQ(-1, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount(-100)).Ticks());
  EXPECT_EQ(-2, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount(-101)).Ticks());
  // The ends of the range
  EXPECT_EQ(92233720368547758, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount(std::numeric_limits<int64_t>::max())).Ticks());
  EXPECT_EQ(-92233720368547759, NanosecondTickCountUtil::ToTickCount(NanosecondTickCount(std::numeric_limits<int64_t>::min())).Ticks());
}


TEST(TestTime_NanosecondTickCountUtil, FromTickCount)
{
  EXPECT_EQ(0, NanosecondTickCountUtil::FromTickCount(TickCount()).TotalNanoseconds());
  EXPECT_EQ(100, NanosecondTickCountUtil::FromTickCount(TickCount(1)).TotalNanoseconds());
  EXPECT_EQ(-100, NanosecondTickCountUtil::FromTickCount(TickCount(-1)).TotalNanoseconds());
  EXPECT_EQ(NanosecondTickCount::NanosecondsPerDay, NanosecondTickCountUtil::FromTickCount(TickCount(TickCount::TicksPerDay)).TotalNanoseconds());
}


TEST(TestTime_NanosecondTickCountUtil, FromTickCount_OutOfRange)
{
  constexpr int64_t MaxTicks = std::numeric_limits<int64_t>::max() / 100;
  constexpr int64_t MinTicks = std::numeric_limits<int64_t>::min() / 100;
  EXPECT_EQ(MaxTicks * 100, NanosecondTickCountUtil::FromTickCount(TickCount(MaxTicks)).TotalNanoseconds());
  EXPECT_EQ(MinTicks * 100, NanosecondTickCountUtil::FromTickCount(TickCount(MinTicks)).TotalNanoseconds());
  EXPECT_THROW(NanosecondTickCountUtil::FromTickCount(TickCount(MaxTicks + 1)), OverflowException);
  EXPECT_THROW(NanosecondTickCountUtil::FromTickCount(TickCount(MinTicks - 1)), UnderflowException);
}


TEST(TestTime_NanosecondTickCountUtil, ToTickCount_FromTickCount_RoundTrip)
{
  for (const int64_t ticks : {int64_t{0}, int64_t{1}, int64_t{-1}, int64_t{41664}, TickCount::TicksPerDay, -TickCount::TicksPerHour})
  {
    EXPECT_EQ(ticks, NanosecondTickCountUtil::ToTickCount(NanosecondTickCountUtil::FromTickCount(TickCount(ticks))).Ticks());
  }
}
