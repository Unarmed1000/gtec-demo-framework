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
#include <Shared/FramePacing/RaymarchParams.hpp>
#include <Shared/FramePacing/SampleBoxAnimation.hpp>
#include <Shared/FramePacing/SampleConfig.hpp>
#include <Shared/FramePacing/SamplePacerAim.hpp>
#include <Shared/FramePacing/SamplePacerKind.hpp>
#include <cstdint>
#include <optional>

namespace Fsl
{
  //! The command line options of the FramePacing samples
  class OptionParser : public ADemoOptionParser
  {
    bool m_hideMarkerStats{false};
    bool m_hidePacingStats{false};
    bool m_hideWorkChart{false};
    bool m_hideAnimationErrorChart{false};
    bool m_hideTestPattern{false};
    SampleBoxAnimationSpeed m_boxAnimation{SampleBoxAnimationSpeed::Off};
    int32_t m_timedRunSeconds{SampleConfig::TimedRunSeconds.Get()};
    bool m_pacerEnabled{false};
    //! The best tier the samples reach: a app that lacks what it uses is paced with the best kind of what it has
    SamplePacerKind m_pacerKind{SamplePacerKind::VBlankWaitForPresent};
    SamplePacerAim m_pacerAim{SamplePacerAim::Smoothness};
    int32_t m_pacerWaitingPresents{SampleConfig::WaitingPresents.Get()};
    int32_t m_pacerReadyPlacePercent{SampleConfig::ReadyPlacePercent.Get()};
    int32_t m_pacerKindChangeFrames{SampleConfig::KindChangeFrames.Get()};
    bool m_pacerSystemHoldsLoop{false};
    bool m_pacerTimedPresent{false};
    bool m_pacerGpuWait{false};
    bool m_pacerSystemWaits{true};
    std::optional<double> m_pacerRefreshRateHz;
    int32_t m_pacerTargetFps{SampleConfig::TargetFps.Get()};
    bool m_pacerAdaptive{true};
    bool m_pacerDisplayReports{false};
    int32_t m_pacerStartupPauseRefreshes{SampleConfig::StartupPauseRefreshes.Get()};
    int32_t m_cpuLoadMs{SampleConfig::CpuLoadMs.Get()};
    int32_t m_cpuSpikeMs{SampleConfig::CpuSpikeMs.Get()};
    int32_t m_cpuSpikeIntervalFrames{SampleConfig::CpuSpikeIntervalFrames.Get()};
    int32_t m_gpuLoadSteps{SampleConfig::GpuLoadSteps.Get()};
    RaymarchScene m_background{RaymarchScene::Mandelbrot};
    bool m_glFlush{false};

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

    //! @brief Check if the sample starts with the chart of the work per frame hidden.
    [[nodiscard]] bool IsWorkChartHidden() const noexcept
    {
      return m_hideWorkChart;
    }

    //! @brief Check if the sample starts with the chart of the animation error hidden.
    [[nodiscard]] bool IsAnimationErrorChartHidden() const noexcept
    {
      return m_hideAnimationErrorChart;
    }

    //! @brief Check if the sample starts with the test pattern (the moving bar and box) hidden.
    [[nodiscard]] bool IsTestPatternHidden() const noexcept
    {
      return m_hideTestPattern;
    }

    //! @brief Get the speed of the box animation of the mb-framepacing-explained videos (Off: it is not shown).
    [[nodiscard]] SampleBoxAnimationSpeed GetBoxAnimation() const noexcept
    {
      return m_boxAnimation;
    }

    //! @brief The duration of a timed run in seconds.
    [[nodiscard]] int32_t GetTimedRunSeconds() const noexcept
    {
      return m_timedRunSeconds;
    }

    //! @brief Check if the sample starts with its frame pacer on.
    [[nodiscard]] bool IsPacerEnabled() const noexcept
    {
      return m_pacerEnabled;
    }

    //! @brief What the pacer of the library is asked to pace the frames with.
    [[nodiscard]] SamplePacerKind GetPacerKind() const noexcept
    {
      return m_pacerKind;
    }

    //! @brief What the pacer optimizes for.
    [[nodiscard]] SamplePacerAim GetPacerAim() const noexcept
    {
      return m_pacerAim;
    }

    //! @brief The presents the pacer lets wait while a frame is made (zero: not said).
    [[nodiscard]] int32_t GetPacerWaitingPresents() const noexcept
    {
      return m_pacerWaitingPresents;
    }

    //! @brief Where in a refresh the pacer of the vertical blank times has a frame ready, in percent of the refresh.
    [[nodiscard]] int32_t GetPacerReadyPlacePercent() const noexcept
    {
      return m_pacerReadyPlacePercent;
    }

    //! @brief The frames between two changes of the pacer kind during a run (zero: the kind is not changed).
    [[nodiscard]] int32_t GetPacerKindChangeFrames() const noexcept
    {
      return m_pacerKindChangeFrames;
    }

    //! @brief True if the pacer is to use the present that takes a time, where the app has one.
    [[nodiscard]] bool IsPacerTimedPresent() const noexcept
    {
      return m_pacerTimedPresent;
    }

    //! @brief True if the pacer is to hold the loop with a wait for the GPU's work, where the app can make that wait.
    [[nodiscard]] bool IsPacerGpuWait() const noexcept
    {
      return m_pacerGpuWait;
    }

    //! @brief True if the waits the app base makes by itself before a frame are reported to the pacer (the default).
    [[nodiscard]] bool IsPacerSystemWaits() const noexcept
    {
      return m_pacerSystemWaits;
    }

    //! @brief True if the pacer is told that the system holds the frame loop while its queue of frames is full.
    [[nodiscard]] bool IsPacerSystemHoldsLoop() const noexcept
    {
      return m_pacerSystemHoldsLoop;
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

    //! @brief Check if the frame pacer is given when the frames were shown, where the app measures that.
    [[nodiscard]] bool IsPacerDisplayReports() const noexcept
    {
      return m_pacerDisplayReports;
    }

    //! @brief Get the refreshes of the pause the pacer makes once after it started.
    [[nodiscard]] int32_t GetPacerStartupPauseRefreshes() const noexcept
    {
      return m_pacerStartupPauseRefreshes;
    }

    //! @brief Check if a OpenGL ES sample calls glFlush after the last command of a frame.
    [[nodiscard]] bool IsGLFlushEnabled() const noexcept
    {
      return m_glFlush;
    }

    //! @brief The time the app spends busy every frame in milliseconds.
    [[nodiscard]] int32_t GetCpuLoadMs() const noexcept
    {
      return m_cpuLoadMs;
    }

    //! @brief The time one frame in every GetCpuSpikeIntervalFrames is busy on top of the CPU load, in milliseconds (0 = no such frame).
    [[nodiscard]] int32_t GetCpuSpikeMs() const noexcept
    {
      return m_cpuSpikeMs;
    }

    //! @brief The number of frames from one frame that runs long to the next.
    [[nodiscard]] int32_t GetCpuSpikeIntervalFrames() const noexcept
    {
      return m_cpuSpikeIntervalFrames;
    }

    //! @brief The number of steps the raymarched background takes for every pixel (0 = no background).
    [[nodiscard]] int32_t GetGpuLoadSteps() const noexcept
    {
      return m_gpuLoadSteps;
    }

    //! @brief The scene of the raymarched background.
    [[nodiscard]] RaymarchScene GetBackground() const noexcept
    {
      return m_background;
    }

  protected:
    void OnArgumentSetup(std::deque<Option>& rOptions) override;
    OptionParseResult OnParse(const int32_t cmdId, const StringViewLite& strOptArg) override;
    bool OnParsingComplete() override;
  };
}

#endif
