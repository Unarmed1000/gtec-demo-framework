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
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingLogFormatter.hpp>
#include <limits>
#include <string>
#include <vector>

using namespace Fsl;

namespace
{
  using Test_FramePacingLogFormatter = TestFixtureFslBase;

  std::vector<FramePacingLogColumnInfo> CreateColumns()
  {
    std::vector<FramePacingLogColumnInfo> columns(3);
    columns[0].Name = "cpuStartTicks";
    columns[1].Name = "rawNanoseconds";
    columns[1].IsUnsigned = true;
    columns[2].Name = "change";
    return columns;
  }
}


TEST(Test_FramePacingLogFormatter, AppendFramesHeader)
{
  std::string text;

  FramePacingLogFormatter::AppendFramesHeader(text, CreateColumns());

  EXPECT_EQ("frameIndex,cpuStartTicks,rawNanoseconds,change\n", text);
}


TEST(Test_FramePacingLogFormatter, AppendFramesHeader_NoColumns)
{
  std::string text;

  FramePacingLogFormatter::AppendFramesHeader(text, {});

  EXPECT_EQ("frameIndex\n", text);
}


TEST(Test_FramePacingLogFormatter, AppendRow)
{
  FramePacingLogRow row;
  row.FrameIndex = 42;
  row.Values[0] = 305358398156;
  row.HasValue.set(0);
  row.Values[2] = -1;
  row.HasValue.set(2);
  std::string text;

  FramePacingLogFormatter::AppendRow(text, row, CreateColumns());

  // A column without a value is empty
  EXPECT_EQ("42,305358398156,,-1\n", text);
}


TEST(Test_FramePacingLogFormatter, AppendRow_AllEmpty)
{
  FramePacingLogRow row;
  row.FrameIndex = 7;
  std::string text;

  FramePacingLogFormatter::AppendRow(text, row, CreateColumns());

  EXPECT_EQ("7,,,\n", text);
}


TEST(Test_FramePacingLogFormatter, AppendRow_TheLimitsOf64Bit)
{
  FramePacingLogRow row;
  row.FrameIndex = std::numeric_limits<uint64_t>::max();
  row.Values[0] = std::numeric_limits<int64_t>::min();
  row.HasValue.set(0);
  // A unsigned column holds the bits of the value
  row.Values[1] = static_cast<int64_t>(std::numeric_limits<uint64_t>::max());
  row.HasValue.set(1);
  row.Values[2] = std::numeric_limits<int64_t>::max();
  row.HasValue.set(2);
  std::string text;

  FramePacingLogFormatter::AppendRow(text, row, CreateColumns());

  EXPECT_EQ("18446744073709551615,-9223372036854775808,18446744073709551615,9223372036854775807\n", text);
}


TEST(Test_FramePacingLogFormatter, AppendRow_AppendsToWhatIsThere)
{
  FramePacingLogRow row;
  row.FrameIndex = 1;
  std::string text("x\n");

  FramePacingLogFormatter::AppendRow(text, row, {});

  EXPECT_EQ("x\n1\n", text);
}


TEST(Test_FramePacingLogFormatter, AppendEventsHeader)
{
  std::string text;

  FramePacingLogFormatter::AppendEventsHeader(text);

  EXPECT_EQ("frameIndex,timeTicks,event,details\n", text);
}


TEST(Test_FramePacingLogFormatter, AppendEvent)
{
  std::string text;

  FramePacingLogFormatter::AppendEvent(text, 12, TickCount(3456), "swapchainCreated", "width=1600;height=900");

  EXPECT_EQ("12,3456,swapchainCreated,width=1600;height=900\n", text);
}


TEST(Test_FramePacingLogFormatter, AppendEvent_QuotesWhatNeedsIt)
{
  std::string text;

  FramePacingLogFormatter::AppendEvent(text, 0, TickCount(1), "fact", "gpu=NVIDIA GeForce, the \"fast\" one");

  EXPECT_EQ("0,1,fact,\"gpu=NVIDIA GeForce, the \"\"fast\"\" one\"\n", text);
}


TEST(Test_FramePacingLogFormatter, AppendField)
{
  std::string text;

  FramePacingLogFormatter::AppendField(text, "plain text; with = signs");
  text.push_back('|');
  FramePacingLogFormatter::AppendField(text, "");
  text.push_back('|');
  FramePacingLogFormatter::AppendField(text, "a,b");
  text.push_back('|');
  FramePacingLogFormatter::AppendField(text, "line\nbreak");
  text.push_back('|');
  FramePacingLogFormatter::AppendField(text, "\"");

  EXPECT_EQ("plain text; with = signs||\"a,b\"|\"line\nbreak\"|\"\"\"\"", text);
}


TEST(Test_FramePacingLogFormatter, ToString_Unit)
{
  EXPECT_EQ("ticks", FramePacingLogFormatter::ToString(FramePacingLogUnit::Ticks));
  EXPECT_EQ("durationTicks", FramePacingLogFormatter::ToString(FramePacingLogUnit::DurationTicks));
  EXPECT_EQ("nanoseconds", FramePacingLogFormatter::ToString(FramePacingLogUnit::Nanoseconds));
  EXPECT_EQ("count", FramePacingLogFormatter::ToString(FramePacingLogUnit::Count));
  EXPECT_EQ("id", FramePacingLogFormatter::ToString(FramePacingLogUnit::Id));
  EXPECT_EQ("flag", FramePacingLogFormatter::ToString(FramePacingLogUnit::Flag));
  EXPECT_EQ("pixels", FramePacingLogFormatter::ToString(FramePacingLogUnit::Pixels));
  EXPECT_EQ("code", FramePacingLogFormatter::ToString(FramePacingLogUnit::Code));
}
