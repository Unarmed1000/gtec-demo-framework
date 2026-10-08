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
#include <FslDemoService/Trace/Impl/TraceFrameTable.hpp>
#include <string>

using namespace Fsl;

namespace
{
  using Test_TraceFrameTable = TestFixtureFslBase;

  constexpr uint32_t OpenRows = TraceFrameTable::OpenRowCount;
}


TEST(Test_TraceFrameTable, RegisterValue)
{
  TraceFrameTable table;

  const TraceValue first = table.RegisterValue("presentCallTicks", TraceUnit::Ticks, "When present was called");
  const TraceValue second = table.RegisterValue("imageIndex", TraceUnit::Id, "The image");

  EXPECT_TRUE(first.IsValid());
  EXPECT_TRUE(second.IsValid());
  EXPECT_NE(first, second);
  ASSERT_EQ(2u, table.GetValues().size());
  EXPECT_EQ("presentCallTicks", table.GetValues()[0].Name);
  EXPECT_EQ(TraceUnit::Ticks, table.GetValues()[0].Unit);
  EXPECT_EQ("When present was called", table.GetValues()[0].Description);
  EXPECT_TRUE(table.IsTime(first));
  EXPECT_FALSE(table.IsTime(second));
}


TEST(Test_TraceFrameTable, RegisterValue_SameNameIsTheSameValue)
{
  TraceFrameTable table;

  const TraceValue first = table.RegisterValue("a", TraceUnit::Count, "first");
  const TraceValue again = table.RegisterValue("a", TraceUnit::Ticks, "second");

  EXPECT_EQ(first, again);
  ASSERT_EQ(1u, table.GetValues().size());
  // The first registration decides what the value is
  EXPECT_EQ(TraceUnit::Count, table.GetValues()[0].Unit);
}


TEST(Test_TraceFrameTable, RegisterValue_BadNameOrLocked)
{
  TraceFrameTable table;

  EXPECT_FALSE(table.RegisterValue("", TraceUnit::Count, "").IsValid());
  EXPECT_FALSE(table.RegisterValue("with space", TraceUnit::Count, "").IsValid());
  EXPECT_FALSE(table.RegisterValue("with,comma", TraceUnit::Count, "").IsValid());

  const TraceValue known = table.RegisterValue("known", TraceUnit::Count, "");
  table.LockValues();
  EXPECT_FALSE(table.RegisterValue("late", TraceUnit::Count, "").IsValid());
  // A value that exists is still found
  EXPECT_EQ(known, table.RegisterValue("known", TraceUnit::Count, ""));
}


TEST(Test_TraceFrameTable, FindValue)
{
  TraceFrameTable table;
  const TraceValue value = table.RegisterValue("a.b", TraceUnit::Count, "");

  EXPECT_EQ(value, table.FindValue("a.b"));
  EXPECT_FALSE(table.FindValue("missing").IsValid());
}


TEST(Test_TraceFrameTable, RegisterValue_NoRoomForMore)
{
  TraceFrameTable table;
  for (uint32_t i = 0; i < TraceFrameRow::MaxValues; ++i)
  {
    EXPECT_TRUE(table.RegisterValue("v" + std::to_string(i), TraceUnit::Count, "").IsValid());
  }
  EXPECT_FALSE(table.RegisterValue("oneTooMany", TraceUnit::Count, "").IsValid());
}


TEST(Test_TraceFrameTable, SetValue_OfTheFrameAndOfAEarlierFrame)
{
  TraceFrameTable table;
  const TraceValue value = table.RegisterValue("a", TraceUnit::Count, "");
  TraceFrameRow closed;

  EXPECT_FALSE(table.BeginFrame(10, 7, closed));
  EXPECT_FALSE(table.BeginFrame(11, 7, closed));
  table.SetValue(11, value, 110);
  // The value of a earlier frame arrives later
  table.SetValue(10, value, 100);

  ASSERT_TRUE(table.TryCloseOldest(closed));
  EXPECT_EQ(10u, closed.FrameIndex);
  EXPECT_EQ(7u, closed.RunId);
  EXPECT_TRUE(closed.HasValue[0]);
  EXPECT_EQ(100, closed.Values[0]);
  ASSERT_TRUE(table.TryCloseOldest(closed));
  EXPECT_EQ(11u, closed.FrameIndex);
  EXPECT_EQ(110, closed.Values[0]);
  EXPECT_FALSE(table.TryCloseOldest(closed));
}


TEST(Test_TraceFrameTable, BeginFrame_ClosesTheRowThatIsTooOld)
{
  TraceFrameTable table;
  const TraceValue value = table.RegisterValue("a", TraceUnit::Count, "");
  TraceFrameRow closed;

  for (uint32_t i = 0; i < OpenRows; ++i)
  {
    EXPECT_FALSE(table.BeginFrame(i, 1, closed));
    table.SetValue(i, value, static_cast<int64_t>(i) * 2);
  }
  // The next frame needs the place of the first one
  ASSERT_TRUE(table.BeginFrame(OpenRows, 1, closed));
  EXPECT_EQ(0u, closed.FrameIndex);
  EXPECT_EQ(0, closed.Values[0]);
  EXPECT_FALSE(table.IsOpen(0));
  EXPECT_TRUE(table.IsOpen(1));

  // A value for the closed frame is ignored, and does not land in the frame that has its place now
  table.SetValue(0, value, 999);
  ASSERT_TRUE(table.BeginFrame(OpenRows + 1u, 1, closed));
  EXPECT_EQ(1u, closed.FrameIndex);
  EXPECT_EQ(2, closed.Values[0]);
}


TEST(Test_TraceFrameTable, SetValue_AValueThatWasNotSetIsMissing)
{
  TraceFrameTable table;
  const TraceValue first = table.RegisterValue("a", TraceUnit::Count, "");
  const TraceValue second = table.RegisterValue("b", TraceUnit::Count, "");
  TraceFrameRow closed;

  table.BeginFrame(0, 1, closed);
  table.SetValue(0, second, 5);
  // Not a value of the table
  table.SetValue(0, TraceValue(), 1);
  table.SetValue(0, TraceValue(99), 1);

  ASSERT_TRUE(table.TryCloseOldest(closed));
  EXPECT_FALSE(closed.HasValue[first.Value - 1u]);
  EXPECT_TRUE(closed.HasValue[second.Value - 1u]);
}
