#ifndef SHARED_FRAMEPACING_OPTIONPARSER_HPP
#define SHARED_FRAMEPACING_OPTIONPARSER_HPP
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

#include <FslDemoApp/Base/ADemoOptionParser.hpp>
#include <Shared/FramePacing/SampleConfig.hpp>
#include <cstdint>
#include <optional>

namespace Fsl
{
  //! The command line options of the FramePacing samples
  class OptionParser : public ADemoOptionParser
  {
    bool m_hideMarkerStats{false};
    bool m_hidePacingStats{false};
    bool m_pacerEnabled{false};
    std::optional<double> m_pacerRefreshRateHz;
    int32_t m_pacerTargetFps{SampleConfig::TargetFps.Get()};
    bool m_pacerAdaptive{true};
    int32_t m_cpuLoadMs{SampleConfig::CpuLoadMs.Get()};
    int32_t m_gpuLoadSteps{SampleConfig::GpuLoadSteps.Get()};

  public:
    OptionParser();
    ~OptionParser() override;

    //! @brief Check if the sample starts with the overlay with the values of the last frame pacing marker hidden.
    [[nodiscard]] bool IsMarkerStatsHidden() const noexcept
    {
      return m_hideMarkerStats;
    }

    //! @brief Check if the sample starts with the overlay with the frame pacing stats hidden.
    [[nodiscard]] bool IsPacingStatsHidden() const noexcept
    {
      return m_hidePacingStats;
    }

    //! @brief Check if the sample starts with its frame pacer on.
    [[nodiscard]] bool IsPacerEnabled() const noexcept
    {
      return m_pacerEnabled;
    }

    //! @brief The refresh rate of the display in Hz the frame pacer should use (empty: ask the window system, else the UI slider).
    [[nodiscard]] std::optional<double> GetPacerRefreshRateHz() const noexcept
    {
      return m_pacerRefreshRateHz;
    }

    //! @brief The frame rate the app wants to run at (0 = the refresh rate of the display).
    [[nodiscard]] int32_t GetPacerTargetFps() const noexcept
    {
      return m_pacerTargetFps;
    }

    //! @brief Check if the frame pacer adapts its swap interval to how the frames do.
    [[nodiscard]] bool IsPacerAdaptive() const noexcept
    {
      return m_pacerAdaptive;
    }

    //! @brief The time the app spends busy every frame in milliseconds.
    [[nodiscard]] int32_t GetCpuLoadMs() const noexcept
    {
      return m_cpuLoadMs;
    }

    //! @brief The number of steps the raymarched background takes for every pixel (0 = no background).
    [[nodiscard]] int32_t GetGpuLoadSteps() const noexcept
    {
      return m_gpuLoadSteps;
    }

  protected:
    void OnArgumentSetup(std::deque<Option>& rOptions) override;
    OptionParseResult OnParse(const int32_t cmdId, const StringViewLite& strOptArg) override;
    bool OnParsingComplete() override;
  };
}

#endif
