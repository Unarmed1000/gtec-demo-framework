#ifndef SHARED_FRAMEPACING_SAMPLEPACER_HPP
#define SHARED_FRAMEPACING_SAMPLEPACER_HPP
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

#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <cstdint>
#include <memory>

namespace Fsl
{
  //! How the sample's frame pacer is set up
  struct SamplePacerConfig
  {
    //! The refresh rate of the display in Hz
    double RefreshRateHz{60.0};
    //! The frame rate the app wants to run at (0 = the refresh rate of the display)
    uint32_t TargetFps{0};
    //! Adapt the swap interval to how the frames do (false = always the swap interval of the target frame rate)
    bool Adaptive{true};

    constexpr bool operator==(const SamplePacerConfig&) const noexcept = default;
  };

  //! What the swap interval rule of the pacer decided for a frame
  enum class SamplePacerChange
  {
    //! The swap interval is the one of the frame before (not called None, that is a macro of the X11 headers)
    Unchanged,
    //! A longer swap interval: a lower frame rate
    Slower,
    //! A shorter swap interval: a higher frame rate
    Faster
  };

  //! What the pacer plans for a frame
  struct SamplePacerSchedule
  {
    //! The number of display refreshes the frame is held for
    uint32_t SwapInterval{1};
    //! The time from the animation time of the previous frame to the one of this frame
    TimeSpan AnimationStep;
    //! When the pacer aims for the frame to be shown (a HighResolutionTimer timestamp)
    TickCount IntendedDisplayTime;
    //! The frame time the pacer aims for: the swap interval as a time
    TimeSpan TargetFrameTime;
    //! The frame time the app wants: the swap interval of the target frame rate as a time
    TimeSpan PreferredFrameTime;
    SamplePacerChange Change{SamplePacerChange::Unchanged};
  };

  //! The frames the swap interval rule of the pacer decides on
  struct SamplePacerStatus
  {
    uint32_t SwapInterval{1};
    //! The swap interval of the target frame rate: the rule never runs faster than this
    uint32_t PreferredSwapInterval{1};
    uint32_t Frames{0};
    uint32_t LateFrames{0};
    //! The average time the frames needed
    TimeSpan AverageWork;
    //! The time the frames span: from when the oldest was shown to when the newest was
    TimeSpan WindowSpan;
    //! True if the rule has the frames it needs to decide on
    bool WindowFull{false};
  };

  //! The mb-framepacing frame pacer (experimental) behind the types of the framework, so the rest of the sample does not depend on the
  //! library. It paces a frame loop with nothing but a steady clock and a present that waits for vsync: every frame it is given the time
  //! the frame starts and it returns the swap interval to hold the frame for and the time step to animate it by.
  class SamplePacer final
  {
    struct Impl;
    std::unique_ptr<Impl> m_impl;

  public:
    SamplePacer(const SamplePacer&) = delete;
    SamplePacer& operator=(const SamplePacer&) = delete;

    //! @return true if the pacer library is available on this platform
    [[nodiscard]] static bool IsSupported() noexcept;

    explicit SamplePacer(const SamplePacerConfig& config);
    ~SamplePacer();

    //! @brief Change how the pacer is set up. With another config the pacer starts again: its frame window is empty and it is back at
    //!        the swap interval of the target frame rate, the animation goes on. The config it has changes nothing.
    void SetConfig(const SamplePacerConfig& config);

    //! @brief Start a frame.
    //! @param cpuStartTime the time the frame starts (a HighResolutionTimer timestamp)
    SamplePacerSchedule BeginFrame(const TickCount cpuStartTime) noexcept;

    //! @brief The frame is about to be presented.
    //! @param presentTime the time now (a HighResolutionTimer timestamp)
    //! @param work how long the frame needed (the CPU time and the GPU time), zero = the time from the frame start to presentTime
    void EndFrame(const TickCount presentTime, const TimeSpan work) noexcept;

    [[nodiscard]] SamplePacerStatus GetStatus() const noexcept;
  };
}

#endif
