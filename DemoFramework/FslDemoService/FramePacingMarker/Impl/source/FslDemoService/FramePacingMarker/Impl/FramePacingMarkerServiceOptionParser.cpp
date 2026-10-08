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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/String/StringParseUtil.hpp>
#include <FslDemoService/FramePacingMarker/IFramePacingMarkerService.hpp>
#include <FslDemoService/FramePacingMarker/Impl/FramePacingMarkerServiceOptionParser.hpp>
#include <fmt/format.h>
#include <cmath>
#include <string_view>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr int32_t MaxModuleSizePx = 1024;
      constexpr int32_t MaxCaptureHeightPx = 16384;
      constexpr double MaxDurationSeconds = 60.0 * 60.0 * 24.0;
    }

    struct CommandId
    {
      enum Enum
      {
        Enable,
        ModuleSize,
        CaptureHeight,
        SyncMarker,
        Run,
        Duration,
        RunId,
      };
    };
  }


  FramePacingMarkerServiceOptionParser::FramePacingMarkerServiceOptionParser()
    : m_moduleSizePx(IFramePacingMarkerService::DefaultModuleSizePx)
  {
  }


  void FramePacingMarkerServiceOptionParser::OnArgumentSetup(std::deque<Option>& rOptions)
  {
    rOptions.emplace_back("FramePacing", OptionArgument::OptionNone, CommandId::Enable,
                          "Draw the mb-framepacing frame marker (frame index + animation time as a QR code) on top of every frame.");
    rOptions.emplace_back(
      "FramePacing.ModuleSize", OptionArgument::OptionRequired, CommandId::ModuleSize,
      fmt::format("The size of one frame pacing marker QR module in pixels. Defaults to: {}", IFramePacingMarkerService::DefaultModuleSizePx));
    rOptions.emplace_back("FramePacing.CaptureHeight", OptionArgument::OptionRequired, CommandId::CaptureHeight,
                          "The height in pixels the capture is stored at, when set the module size is calculated so the marker survives the "
                          "downscale (overrides FramePacing.ModuleSize).");
    rOptions.emplace_back("FramePacing.SyncMarker", OptionArgument::OptionNone, CommandId::SyncMarker,
                          "Also draw the small frame pacing sync marker at the bottom left: it detects tearing, and camera capture needs it for "
                          "its timing.");
    rOptions.emplace_back("FramePacing.Run", OptionArgument::OptionRequired, CommandId::Run,
                          "Start a measured run with the given name at the first frame, the name is written to the trace next to the run's "
                          "random sequence id. Implies --FramePacing.");
    rOptions.emplace_back("FramePacing.Duration", OptionArgument::OptionRequired, CommandId::Duration,
                          "The duration in seconds of the measured part of the run started by FramePacing.Run (0 = until the app exits).");
    rOptions.emplace_back("FramePacing.RunId", OptionArgument::OptionRequired, CommandId::RunId,
                          "The id of the run started by FramePacing.Run (defaults to a random id).");
  }


  OptionParseResult FramePacingMarkerServiceOptionParser::OnParse(const int32_t cmdId, const StringViewLite& strOptArg)
  {
    switch (cmdId)
    {
    case CommandId::Enable:
      m_enabled = true;
      return OptionParseResult::Parsed;
    case CommandId::ModuleSize:
      {
        int32_t value = 0;
        StringParseUtil::Parse(value, strOptArg);
        if (value < 1 || value > LocalConfig::MaxModuleSizePx)
        {
          FSLLOG3_ERROR("FramePacing.ModuleSize must be in the range [1,{}]", LocalConfig::MaxModuleSizePx);
          return OptionParseResult::Failed;
        }
        m_moduleSizePx = value;
        return OptionParseResult::Parsed;
      }
    case CommandId::CaptureHeight:
      {
        int32_t value = 0;
        StringParseUtil::Parse(value, strOptArg);
        if (value < 1 || value > LocalConfig::MaxCaptureHeightPx)
        {
          FSLLOG3_ERROR("FramePacing.CaptureHeight must be in the range [1,{}]", LocalConfig::MaxCaptureHeightPx);
          return OptionParseResult::Failed;
        }
        m_captureHeightPx = value;
        return OptionParseResult::Parsed;
      }
    case CommandId::SyncMarker:
      m_syncMarkerEnabled = true;
      return OptionParseResult::Parsed;
    case CommandId::Run:
      m_runName = std::string(std::string_view(strOptArg));
      m_enabled = true;
      return OptionParseResult::Parsed;
    case CommandId::Duration:
      {
        double seconds = 0.0;
        StringParseUtil::Parse(seconds, strOptArg);
        if (!std::isfinite(seconds) || seconds < 0.0 || seconds > LocalConfig::MaxDurationSeconds)
        {
          FSLLOG3_ERROR("FramePacing.Duration must be in the range [0,{}] seconds", LocalConfig::MaxDurationSeconds);
          return OptionParseResult::Failed;
        }
        m_runDuration = TimeSpan(static_cast<int64_t>(std::llround(seconds * static_cast<double>(TimeSpan::TicksPerSecond))));
        return OptionParseResult::Parsed;
      }
    case CommandId::RunId:
      {
        uint32_t value = 0;
        StringParseUtil::Parse(value, strOptArg);
        m_runId = value;
        return OptionParseResult::Parsed;
      }
    default:
      return OptionParseResult::NotHandled;
    }
  }


  bool FramePacingMarkerServiceOptionParser::OnParsingComplete()
  {
    if (!m_runName.has_value() && (m_runDuration > TimeSpan() || m_runId.has_value()))
    {
      FSLLOG3_ERROR("FramePacing.Duration and FramePacing.RunId require FramePacing.Run");
      return false;
    }
    return true;
  }
}
