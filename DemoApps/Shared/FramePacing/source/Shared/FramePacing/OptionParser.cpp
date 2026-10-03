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

#include <FslBase/Getopt/OptionBaseValues.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/String/StringParseUtil.hpp>
#include <Shared/FramePacing/OptionParser.hpp>
#include <cmath>

namespace Fsl
{
  namespace
  {
    struct CommandId
    {
      enum Enum
      {
        HideMarkerStats = DEMO_APP_OPTION_BASE,
        HidePacingStats,
        HideWorkChart,
        HideTestPattern,
        TimedRunDuration,
        Pacer,
        PacerRefreshRate,
        PacerTargetFps,
        PacerAdaptive,
        CpuLoad,
        GpuLoad,
        Background,
        PresentLog
      };
    };

    //! Parse a integer option that has to be inside the range of the constrained value
    bool TryParseInRange(int32_t& rValue, const StringViewLite strOptArg, const ConstrainedValue<int32_t> range, const char* const pszName)
    {
      int32_t value = 0;
      StringParseUtil::Parse(value, strOptArg);
      if (value < range.Min() || value > range.Max())
      {
        FSLLOG3_ERROR("{} must be in the range [{},{}]", pszName, range.Min(), range.Max());
        return false;
      }
      rValue = value;
      return true;
    }
  }


  OptionParser::OptionParser() = default;


  OptionParser::~OptionParser() = default;


  void OptionParser::OnArgumentSetup(std::deque<Option>& rOptions)
  {
    rOptions.emplace_back("HideMarkerStats", OptionArgument::OptionNone, CommandId::HideMarkerStats,
                          "Start with the overlay with the values of the last frame pacing marker hidden (the UI has a switch for it).");
    rOptions.emplace_back("HidePacingStats", OptionArgument::OptionNone, CommandId::HidePacingStats,
                          "Start with the overlay with the frame pacing stats hidden (the UI has a switch for it).");
    rOptions.emplace_back("HideWorkChart", OptionArgument::OptionNone, CommandId::HideWorkChart,
                          "Start with the chart of the work per frame hidden (the UI has a switch for it).");
    rOptions.emplace_back("HideTestPattern", OptionArgument::OptionNone, CommandId::HideTestPattern,
                          "Start with the test pattern (the moving bar and box) hidden (the UI has a switch for it).");
    rOptions.emplace_back("TimedRunDuration", OptionArgument::OptionRequired, CommandId::TimedRunDuration,
                          "The duration in seconds of a timed run that is started in the UI (1 to 120, the default is 10).");
    rOptions.emplace_back("Pacer", OptionArgument::OptionNone, CommandId::Pacer,
                          "Start with the frame pacer of the sample on (the experimental mb-framepacing pacer).");
    rOptions.emplace_back("Pacer.RefreshRate", OptionArgument::OptionRequired, CommandId::PacerRefreshRate,
                          "The refresh rate of the display in Hz the frame pacer uses, decimals are allowed (59.94). Defaults to the "
                          "rate the window system reports, and to the UI slider if it does not know it.");
    rOptions.emplace_back("Pacer.TargetFps", OptionArgument::OptionRequired, CommandId::PacerTargetFps,
                          "The frame rate the frame pacer aims for (0 = the refresh rate of the display, the default).");
    rOptions.emplace_back("Pacer.Adaptive", OptionArgument::OptionRequired, CommandId::PacerAdaptive,
                          "true (default): the frame pacer adapts its swap interval to how the frames do. false: a fixed frame rate.");
    rOptions.emplace_back("CpuLoad", OptionArgument::OptionRequired, CommandId::CpuLoad,
                          "Simulate a CPU load: the time in milliseconds the app spends busy every frame (0 = none, the default).");
    rOptions.emplace_back("GpuLoad", OptionArgument::OptionRequired, CommandId::GpuLoad,
                          "A GPU load: the number of steps the raymarched background takes for every pixel (0 = no background, the "
                          "default).");
    rOptions.emplace_back("Background", OptionArgument::OptionRequired, CommandId::Background,
                          "The scene of the raymarched background: flight (a flight through a fractal lattice, the default) or hall (a "
                          "hall of columns that scrolls sideways at a constant speed, which makes a stutter easy to see).");
    rOptions.emplace_back("PresentLog", OptionArgument::OptionRequired, CommandId::PresentLog,
                          "Write a CSV file with one row per presented frame when the sample exits: when the frame started, where the "
                          "frame loop waited, what the frame pacer planned and when the frame reached the display. All times are in "
                          "100ns ticks. Vulkan only, as it needs the id of the present.");
  }


  OptionParseResult OptionParser::OnParse(const int32_t cmdId, const StringViewLite& strOptArg)
  {
    switch (cmdId)
    {
    case CommandId::HideMarkerStats:
      m_hideMarkerStats = true;
      return OptionParseResult::Parsed;
    case CommandId::HidePacingStats:
      m_hidePacingStats = true;
      return OptionParseResult::Parsed;
    case CommandId::HideWorkChart:
      m_hideWorkChart = true;
      return OptionParseResult::Parsed;
    case CommandId::HideTestPattern:
      m_hideTestPattern = true;
      return OptionParseResult::Parsed;
    case CommandId::TimedRunDuration:
      return TryParseInRange(m_timedRunSeconds, strOptArg, SampleConfig::TimedRunSeconds, "TimedRunDuration") ? OptionParseResult::Parsed
                                                                                                              : OptionParseResult::Failed;
    case CommandId::Pacer:
      m_pacerEnabled = true;
      return OptionParseResult::Parsed;
    case CommandId::PacerRefreshRate:
      {
        double value = 0.0;
        StringParseUtil::Parse(value, strOptArg);
        if (!std::isfinite(value) || value < SampleConfig::MinRefreshRateHz || value > SampleConfig::MaxRefreshRateHz)
        {
          FSLLOG3_ERROR("Pacer.RefreshRate must be in the range [{},{}]", SampleConfig::MinRefreshRateHz, SampleConfig::MaxRefreshRateHz);
          return OptionParseResult::Failed;
        }
        m_pacerRefreshRateHz = value;
        return OptionParseResult::Parsed;
      }
    case CommandId::PacerTargetFps:
      return TryParseInRange(m_pacerTargetFps, strOptArg, SampleConfig::TargetFps, "Pacer.TargetFps") ? OptionParseResult::Parsed
                                                                                                      : OptionParseResult::Failed;
    case CommandId::PacerAdaptive:
      StringParseUtil::Parse(m_pacerAdaptive, strOptArg);
      return OptionParseResult::Parsed;
    case CommandId::CpuLoad:
      return TryParseInRange(m_cpuLoadMs, strOptArg, SampleConfig::CpuLoadMs, "CpuLoad") ? OptionParseResult::Parsed : OptionParseResult::Failed;
    case CommandId::GpuLoad:
      return TryParseInRange(m_gpuLoadSteps, strOptArg, SampleConfig::GpuLoadSteps, "GpuLoad") ? OptionParseResult::Parsed
                                                                                               : OptionParseResult::Failed;
    case CommandId::Background:
      if (strOptArg == "flight")
      {
        m_background = RaymarchScene::Flight;
        return OptionParseResult::Parsed;
      }
      if (strOptArg == "hall")
      {
        m_background = RaymarchScene::Hall;
        return OptionParseResult::Parsed;
      }
      FSLLOG3_ERROR("Background must be 'flight' or 'hall'");
      return OptionParseResult::Failed;
    case CommandId::PresentLog:
      m_presentLogPath = IO::Path(strOptArg);
      return OptionParseResult::Parsed;
    default:
      return OptionParseResult::NotHandled;
    }
  }


  bool OptionParser::OnParsingComplete()
  {
    return true;
  }
}
