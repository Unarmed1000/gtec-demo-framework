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

#include <FslBase/Getopt/Option.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDemoService/FramePacing/IFramePacingService.hpp>
#include <FslDemoService/FramePacing/Impl/FramePacingServiceOptionParser.hpp>
#include <deque>
#include <string>

using namespace Fsl;

namespace
{
  using Test_FramePacingServiceOptionParser = TestFixtureFslBase;

  class ParserHelper
  {
    std::deque<Option> m_options;

  public:
    FramePacingServiceOptionParser Parser;

    ParserHelper()
    {
      Parser.OnArgumentSetup(m_options);
    }

    OptionParseResult Parse(const std::string& name, const StringViewLite value = {})
    {
      for (const auto& option : m_options)
      {
        if (option.Name == name)
        {
          return Parser.OnParse(option.CmdId, value);
        }
      }
      ADD_FAILURE() << "Unknown option " << name;
      return OptionParseResult::NotHandled;
    }
  };
}


TEST(Test_FramePacingServiceOptionParser, Defaults)
{
  ParserHelper helper;
  EXPECT_TRUE(helper.Parser.OnParsingComplete());

  EXPECT_FALSE(helper.Parser.IsEnabled());
  EXPECT_EQ(IFramePacingService::DefaultModuleSizePx, helper.Parser.GetModuleSizePx());
  EXPECT_EQ(0, helper.Parser.GetCaptureHeightPx());
  EXPECT_EQ(FramePacingMarkerSlot::TopLeft, helper.Parser.GetSlot());
  EXPECT_FALSE(helper.Parser.GetRunName().has_value());
  EXPECT_EQ(TimeSpan(), helper.Parser.GetRunDuration());
  EXPECT_FALSE(helper.Parser.GetRunId().has_value());
}


TEST(Test_FramePacingServiceOptionParser, Enable)
{
  ParserHelper helper;
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing"));
  EXPECT_TRUE(helper.Parser.IsEnabled());
}


TEST(Test_FramePacingServiceOptionParser, ModuleSize)
{
  ParserHelper helper;
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing.ModuleSize", "9"));
  EXPECT_EQ(9, helper.Parser.GetModuleSizePx());
  EXPECT_EQ(OptionParseResult::Failed, helper.Parse("FramePacing.ModuleSize", "0"));
  EXPECT_EQ(OptionParseResult::Failed, helper.Parse("FramePacing.ModuleSize", "1025"));
  EXPECT_EQ(9, helper.Parser.GetModuleSizePx());
}


TEST(Test_FramePacingServiceOptionParser, CaptureHeight)
{
  ParserHelper helper;
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing.CaptureHeight", "540"));
  EXPECT_EQ(540, helper.Parser.GetCaptureHeightPx());
  EXPECT_EQ(OptionParseResult::Failed, helper.Parse("FramePacing.CaptureHeight", "0"));
}


TEST(Test_FramePacingServiceOptionParser, Slot)
{
  ParserHelper helper;
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing.Slot", "middle"));
  EXPECT_EQ(FramePacingMarkerSlot::MiddleLeft, helper.Parser.GetSlot());
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing.Slot", "bottom"));
  EXPECT_EQ(FramePacingMarkerSlot::BottomLeft, helper.Parser.GetSlot());
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing.Slot", "all"));
  EXPECT_EQ(FramePacingMarkerSlot::All, helper.Parser.GetSlot());
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing.Slot", "top"));
  EXPECT_EQ(FramePacingMarkerSlot::TopLeft, helper.Parser.GetSlot());
  EXPECT_EQ(OptionParseResult::Failed, helper.Parse("FramePacing.Slot", "left"));
}


TEST(Test_FramePacingServiceOptionParser, Run)
{
  ParserHelper helper;
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing.Run", "camera pan"));
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing.Duration", "2.5"));
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing.RunId", "7"));
  EXPECT_TRUE(helper.Parser.OnParsingComplete());

  EXPECT_TRUE(helper.Parser.IsEnabled());
  ASSERT_TRUE(helper.Parser.GetRunName().has_value());
  EXPECT_EQ(std::string("camera pan"), helper.Parser.GetRunName().value());
  EXPECT_EQ(TimeSpan(25 * TimeSpan::TicksPerSecond / 10), helper.Parser.GetRunDuration());
  ASSERT_TRUE(helper.Parser.GetRunId().has_value());
  EXPECT_EQ(7u, helper.Parser.GetRunId().value());
}


TEST(Test_FramePacingServiceOptionParser, Run_NameTooLong)
{
  ParserHelper helper;
  const std::string name(IFramePacingService::MaxRunNameBytes + 1, 'x');
  EXPECT_EQ(OptionParseResult::Failed, helper.Parse("FramePacing.Run", StringViewLite(name)));
  EXPECT_FALSE(helper.Parser.GetRunName().has_value());
}


TEST(Test_FramePacingServiceOptionParser, Duration_Invalid)
{
  ParserHelper helper;
  EXPECT_EQ(OptionParseResult::Failed, helper.Parse("FramePacing.Duration", "-1"));
}


TEST(Test_FramePacingServiceOptionParser, Duration_WithoutRun)
{
  ParserHelper helper;
  EXPECT_EQ(OptionParseResult::Parsed, helper.Parse("FramePacing.Duration", "1"));
  EXPECT_FALSE(helper.Parser.OnParsingComplete());
}
