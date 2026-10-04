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
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingFrameLogTable.hpp>
#include <limits>
#include <string>

using namespace Fsl;

namespace
{
  using Test_FramePacingFrameLogTable = TestFixtureFslBase;

  constexpr uint32_t OpenRows = FramePacingFrameLogTable::OpenRowCount;
}


TEST(Test_FramePacingFrameLogTable, RegisterColumn)
{
  FramePacingFrameLogTable table;

  const FramePacingLogColumn first = table.RegisterColumn("presentCallTicks", FramePacingLogUnit::Ticks, "When present was called");
  const FramePacingLogColumn second = table.RegisterColumn("imageIndex", FramePacingLogUnit::Id, "The image");

  EXPECT_TRUE(first.IsValid());
  EXPECT_TRUE(second.IsValid());
  EXPECT_NE(first, second);
  ASSERT_EQ(2u, table.GetColumns().size());
  EXPECT_EQ("presentCallTicks", table.GetColumns()[0].Name);
  EXPECT_EQ(FramePacingLogUnit::Ticks, table.GetColumns()[0].Unit);
  EXPECT_EQ("When present was called", table.GetColumns()[0].Description);
  EXPECT_EQ("imageIndex", table.GetColumns()[1].Name);
}


TEST(Test_FramePacingFrameLogTable, RegisterColumn_SameNameIsTheSameColumn)
{
  FramePacingFrameLogTable table;

  const FramePacingLogColumn first = table.RegisterColumn("a", FramePacingLogUnit::Count, "first");
  const FramePacingLogColumn again = table.RegisterColumn("a", FramePacingLogUnit::Ticks, "second");

  EXPECT_EQ(first, again);
  ASSERT_EQ(1u, table.GetColumns().size());
  // The first registration decides what the column is
  EXPECT_EQ(FramePacingLogUnit::Count, table.GetColumns()[0].Unit);
}


TEST(Test_FramePacingFrameLogTable, RegisterColumn_BadNames)
{
  FramePacingFrameLogTable table;

  EXPECT_FALSE(table.RegisterColumn("", FramePacingLogUnit::Count, "").IsValid());
  EXPECT_FALSE(table.RegisterColumn("a,b", FramePacingLogUnit::Count, "").IsValid());
  EXPECT_FALSE(table.RegisterColumn("a b", FramePacingLogUnit::Count, "").IsValid());
  EXPECT_FALSE(table.RegisterColumn("a\"b", FramePacingLogUnit::Count, "").IsValid());
  EXPECT_FALSE(table.RegisterColumn("a\nb", FramePacingLogUnit::Count, "").IsValid());
  EXPECT_TRUE(table.RegisterColumn("pacer.swapInterval_2", FramePacingLogUnit::Count, "").IsValid());
  EXPECT_EQ(1u, table.GetColumns().size());
}


TEST(Test_FramePacingFrameLogTable, RegisterColumn_Locked)
{
  FramePacingFrameLogTable table;
  const FramePacingLogColumn first = table.RegisterColumn("a", FramePacingLogUnit::Count, "");
  table.LockColumns();

  EXPECT_TRUE(table.IsColumnsLocked());
  EXPECT_FALSE(table.RegisterColumn("b", FramePacingLogUnit::Count, "").IsValid());
  // A known column can still be asked for
  EXPECT_EQ(first, table.RegisterColumn("a", FramePacingLogUnit::Count, ""));
}


TEST(Test_FramePacingFrameLogTable, RegisterColumn_Full)
{
  FramePacingFrameLogTable table;
  for (uint32_t i = 0; i < FramePacingLogRow::MaxColumns; ++i)
  {
    EXPECT_TRUE(table.RegisterColumn("c" + std::to_string(i), FramePacingLogUnit::Count, "").IsValid());
  }

  EXPECT_FALSE(table.RegisterColumn("oneTooMany", FramePacingLogUnit::Count, "").IsValid());
  EXPECT_EQ(FramePacingLogRow::MaxColumns, table.GetColumns().size());
}


TEST(Test_FramePacingFrameLogTable, SetValue_CurrentFrame)
{
  FramePacingFrameLogTable table;
  const FramePacingLogColumn a = table.RegisterColumn("a", FramePacingLogUnit::Count, "");
  const FramePacingLogColumn b = table.RegisterColumn("b", FramePacingLogUnit::Count, "");
  FramePacingLogRow closed;

  EXPECT_FALSE(table.BeginFrame(0, closed));
  table.SetValue(0, a, 42, false);

  ASSERT_TRUE(table.TryCloseOldest(closed));
  EXPECT_EQ(0u, closed.FrameIndex);
  EXPECT_TRUE(closed.HasValue.test(0));
  EXPECT_EQ(42, closed.Values[0]);
  // A column without a value stays empty
  EXPECT_FALSE(closed.HasValue.test(1));
  EXPECT_TRUE(b.IsValid());
  EXPECT_FALSE(table.TryCloseOldest(closed));
}


TEST(Test_FramePacingFrameLogTable, SetValue_EarlierFrame)
{
  FramePacingFrameLogTable table;
  const FramePacingLogColumn a = table.RegisterColumn("a", FramePacingLogUnit::Count, "");
  FramePacingLogRow closed;
  for (uint64_t frame = 0; frame < 5; ++frame)
  {
    EXPECT_FALSE(table.BeginFrame(frame, closed));
  }

  // Something about frame 1 is known while frame 4 is drawn
  table.SetValue(1, a, 1001, false);

  for (uint64_t frame = 0; frame < 5; ++frame)
  {
    ASSERT_TRUE(table.TryCloseOldest(closed));
    EXPECT_EQ(frame, closed.FrameIndex);
    EXPECT_EQ(frame == 1u, closed.HasValue.test(0));
    if (frame == 1u)
    {
      EXPECT_EQ(1001, closed.Values[0]);
    }
  }
}


TEST(Test_FramePacingFrameLogTable, BeginFrame_ClosesTheRowThatIsTooOld)
{
  FramePacingFrameLogTable table;
  const FramePacingLogColumn a = table.RegisterColumn("a", FramePacingLogUnit::Count, "");
  FramePacingLogRow closed;
  for (uint64_t frame = 0; frame < OpenRows; ++frame)
  {
    EXPECT_FALSE(table.BeginFrame(frame, closed));
    table.SetValue(frame, a, static_cast<int64_t>(frame) * 10, false);
  }
  EXPECT_TRUE(table.IsOpen(0));

  // The next frame needs the place of frame 0
  ASSERT_TRUE(table.BeginFrame(OpenRows, closed));
  EXPECT_EQ(0u, closed.FrameIndex);
  EXPECT_TRUE(closed.HasValue.test(0));
  EXPECT_EQ(0, closed.Values[0]);
  EXPECT_FALSE(table.IsOpen(0));
  EXPECT_TRUE(table.IsOpen(1));
  EXPECT_TRUE(table.IsOpen(OpenRows));

  ASSERT_TRUE(table.BeginFrame(OpenRows + 1u, closed));
  EXPECT_EQ(1u, closed.FrameIndex);
  EXPECT_EQ(10, closed.Values[0]);
}


TEST(Test_FramePacingFrameLogTable, SetValue_ClosedFrameIsIgnored)
{
  FramePacingFrameLogTable table;
  const FramePacingLogColumn a = table.RegisterColumn("a", FramePacingLogUnit::Count, "");
  FramePacingLogRow closed;
  for (uint64_t frame = 0; frame <= OpenRows; ++frame)
  {
    table.BeginFrame(frame, closed);
  }

  // Frame 0 was closed, its place belongs to the newest frame now
  table.SetValue(0, a, 99, false);

  while (table.TryCloseOldest(closed))
  {
    EXPECT_FALSE(closed.HasValue.test(0)) << closed.FrameIndex;
  }
}


TEST(Test_FramePacingFrameLogTable, SetValue_NotAColumn)
{
  FramePacingFrameLogTable table;
  table.RegisterColumn("a", FramePacingLogUnit::Count, "");
  FramePacingLogRow closed;
  table.BeginFrame(0, closed);

  table.SetValue(0, FramePacingLogColumn(), 1, false);
  table.SetValue(0, FramePacingLogColumn(2), 1, false);
  table.SetValue(0, FramePacingLogColumn(1000), 1, false);

  ASSERT_TRUE(table.TryCloseOldest(closed));
  EXPECT_TRUE(closed.HasValue.none());
}


TEST(Test_FramePacingFrameLogTable, SetValue_Unsigned)
{
  FramePacingFrameLogTable table;
  const FramePacingLogColumn a = table.RegisterColumn("a", FramePacingLogUnit::Nanoseconds, "");
  const FramePacingLogColumn b = table.RegisterColumn("b", FramePacingLogUnit::Count, "");
  FramePacingLogRow closed;
  table.BeginFrame(0, closed);

  table.SetValue(0, a, static_cast<int64_t>(std::numeric_limits<uint64_t>::max()), true);
  table.SetValue(0, b, -1, false);

  EXPECT_TRUE(table.GetColumns()[0].IsUnsigned);
  EXPECT_FALSE(table.GetColumns()[1].IsUnsigned);
  EXPECT_TRUE(a.IsValid());
  EXPECT_TRUE(b.IsValid());
}


TEST(Test_FramePacingFrameLogTable, TryCloseOldest_Empty)
{
  FramePacingFrameLogTable table;
  FramePacingLogRow closed;

  EXPECT_FALSE(table.TryCloseOldest(closed));
}


TEST(Test_FramePacingFrameLogTable, TryCloseOldest_InFrameOrder_AfterTheRingWrapped)
{
  FramePacingFrameLogTable table;
  FramePacingLogRow closed;
  const uint64_t frames = (OpenRows * 2u) + 7u;
  for (uint64_t frame = 0; frame < frames; ++frame)
  {
    table.BeginFrame(frame, closed);
  }

  uint64_t expected = frames - OpenRows;
  while (table.TryCloseOldest(closed))
  {
    EXPECT_EQ(expected, closed.FrameIndex);
    ++expected;
  }
  EXPECT_EQ(frames, expected);
}
