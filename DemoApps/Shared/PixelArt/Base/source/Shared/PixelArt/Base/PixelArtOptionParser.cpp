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
#include <Shared/PixelArt/Base/PixelArtOptionParser.hpp>
#include <cmath>
#include <string>

namespace Fsl
{
  namespace
  {
    struct CommandId
    {
      enum Enum
      {
        Scene = DEMO_APP_OPTION_BASE,
        RenderScale,
        TimeSpeed,
        Time,
        Paused,
        HideUI,
        GpuChart,
        Param
      };
    };

    bool TryParseFloat(float& rValue, const StringViewLite strOptArg, const float minValue, const float maxValue, const char* const pszName)
    {
      float value = 0.0f;
      StringParseUtil::Parse(value, strOptArg);
      if (!std::isfinite(value) || value < minValue || value > maxValue)
      {
        FSLLOG3_ERROR("{} must be in the range [{},{}]", pszName, minValue, maxValue);
        return false;
      }
      rValue = value;
      return true;
    }

    //! "NAME=value,NAME=value"
    bool TryParseParams(std::vector<std::pair<std::string, float>>& rParams, const StringViewLite strOptArg)
    {
      const std::string text(strOptArg.data(), strOptArg.size());
      std::size_t start = 0;
      while (start <= text.size())
      {
        std::size_t end = text.find(',', start);
        if (end == std::string::npos)
        {
          end = text.size();
        }
        const std::string entry = text.substr(start, end - start);
        const std::size_t separator = entry.find('=');
        if (separator == std::string::npos || separator == 0)
        {
          FSLLOG3_ERROR("Param expects NAME=value, not '{}'", entry);
          return false;
        }
        float value = 0.0f;
        StringParseUtil::Parse(value, StringViewLite(entry.data() + separator + 1, entry.size() - separator - 1));
        rParams.emplace_back(entry.substr(0, separator), value);
        start = end + 1;
      }
      return true;
    }
  }


  PixelArtOptionParser::PixelArtOptionParser() = default;


  PixelArtOptionParser::~PixelArtOptionParser() = default;


  void PixelArtOptionParser::OnArgumentSetup(std::deque<Option>& rOptions)
  {
    rOptions.emplace_back("Scene", OptionArgument::OptionRequired, CommandId::Scene,
                          "The scene to start with: the name of its folder or its number in the list (1 is the first scene).");
    rOptions.emplace_back("RenderScale", OptionArgument::OptionRequired, CommandId::RenderScale,
                          "The size of the buffers relative to the window (0.25 to 1, the default is 1).");
    rOptions.emplace_back("TimeSpeed", OptionArgument::OptionRequired, CommandId::TimeSpeed, "How fast iTime runs (0 to 4, the default is 1).");
    rOptions.emplace_back("Time", OptionArgument::OptionRequired, CommandId::Time,
                          "The iTime the scene starts at in seconds (for screenshots that are the same every time).");
    rOptions.emplace_back("Paused", OptionArgument::OptionNone, CommandId::Paused, "Start with the time paused.");
    rOptions.emplace_back("HideUI", OptionArgument::OptionNone, CommandId::HideUI, "Start with the UI hidden (U shows it).");
    rOptions.emplace_back("GpuChart", OptionArgument::OptionNone, CommandId::GpuChart,
                          "Start with the chart of the GPU time per frame shown (G toggles it).");
    rOptions.emplace_back("Param", OptionArgument::OptionRequired, CommandId::Param,
                          "Set params of the start scene: NAME=value[,NAME=value...] (the names are the defines of its Params.glsl).");
  }


  OptionParseResult PixelArtOptionParser::OnParse(const int32_t cmdId, const StringViewLite& strOptArg)
  {
    switch (cmdId)
    {
    case CommandId::Scene:
      m_scene = std::string(strOptArg.data(), strOptArg.size());
      return OptionParseResult::Parsed;
    case CommandId::RenderScale:
      return TryParseFloat(m_renderScale, strOptArg, 0.25f, 1.0f, "RenderScale") ? OptionParseResult::Parsed : OptionParseResult::Failed;
    case CommandId::TimeSpeed:
      return TryParseFloat(m_timeSpeed, strOptArg, 0.0f, 4.0f, "TimeSpeed") ? OptionParseResult::Parsed : OptionParseResult::Failed;
    case CommandId::Time:
      return TryParseFloat(m_startTime, strOptArg, 0.0f, 100000.0f, "Time") ? OptionParseResult::Parsed : OptionParseResult::Failed;
    case CommandId::Paused:
      m_paused = true;
      return OptionParseResult::Parsed;
    case CommandId::HideUI:
      m_hideUI = true;
      return OptionParseResult::Parsed;
    case CommandId::GpuChart:
      m_gpuChart = true;
      return OptionParseResult::Parsed;
    case CommandId::Param:
      return TryParseParams(m_params, strOptArg) ? OptionParseResult::Parsed : OptionParseResult::Failed;
    default:
      return OptionParseResult::NotHandled;
    }
  }


  bool PixelArtOptionParser::OnParsingComplete()
  {
    return true;
  }
}
