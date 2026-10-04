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

#include <FslBase/BasicTypes.hpp>
#include <Shared/FramePacing/SamplePacer.hpp>
#include <algorithm>
#include <cmath>

// The pacer library is only available on the platforms that support the frame pacing marker library
#ifdef FSL_ENABLE_MB_FRAMEPACING
#include <mb/framepacing/core/time/TickCount64.hpp>
#include <mb/framepacing/core/time/TimeSpan.hpp>
#include <mb/framepacing/pacer/FramePacer.hpp>
#include <mb/framepacing/pacer/PacerSettings.hpp>
#include <mb/framepacing/pacer/RefreshPeriod.hpp>
#include <mb/framepacing/pacer/frame/FrameSchedule.hpp>
#include <mb/framepacing/pacer/frame/PresentFeedback.hpp>
#include <mb/framepacing/pacer/frame/PresentFeedbackState.hpp>
#include <mb/framepacing/pacer/rule/FrameWindowState.hpp>
#include <mb/framepacing/pacer/rule/SwapIntervalChange.hpp>
#endif

namespace Fsl
{
#ifdef FSL_ENABLE_MB_FRAMEPACING
  namespace
  {
    namespace FP = MB::FramePacing;
    namespace PC = MB::FramePacing::Pacer;

    namespace LocalConfig
    {
      // The refresh rates the pacer accepts (a period of 100 microseconds to 1 second)
      constexpr double MinRefreshRateHz = 1.0;
      constexpr double MaxRefreshRateHz = 10000.0;
      constexpr double NanosecondsPerSecond = 1000000000.0;
    }

    PC::PacerSettings ToPacerSettings(const SamplePacerConfig& config) noexcept
    {
      // The refresh period exact to a nanosecond (a period in whole 100ns ticks would drift)
      const double refreshRateHz = std::isfinite(config.RefreshRateHz)
                                     ? std::clamp(config.RefreshRateHz, LocalConfig::MinRefreshRateHz, LocalConfig::MaxRefreshRateHz)
                                     : SamplePacerConfig().RefreshRateHz;
      PC::PacerSettings settings(PC::RefreshPeriod::FromNanoseconds(std::llround(LocalConfig::NanosecondsPerSecond / refreshRateHz)));
      if (config.TargetFps > 0u)
      {
        settings.SetPreferredFrameRate(config.TargetFps);
      }
      settings.SetAutoSwapInterval(config.Adaptive);
      settings.SetUsePresentFeedback(config.PresentFeedback);
      return settings;
    }

    SamplePacerChange ToSamplePacerChange(const PC::SwapIntervalChange change) noexcept
    {
      switch (change)
      {
      case PC::SwapIntervalChange::Slower:
        return SamplePacerChange::Slower;
      case PC::SwapIntervalChange::Faster:
        return SamplePacerChange::Faster;
      case PC::SwapIntervalChange::Unchanged:
      default:
        return SamplePacerChange::Unchanged;
      }
    }
  }

  // The framework and the library both count time in 100ns ticks, and a HighResolutionTimer timestamp is the steady clock the pacer needs
  struct SamplePacer::Impl
  {
    PC::FramePacer Pacer;

    explicit Impl(const SamplePacerConfig& config)
      : Pacer(ToPacerSettings(config))
    {
    }
  };


  bool SamplePacer::IsSupported() noexcept
  {
    return true;
  }


  SamplePacer::SamplePacer(const SamplePacerConfig& config)
    : m_impl(std::make_unique<Impl>(config))
  {
  }


  SamplePacer::~SamplePacer() = default;


  void SamplePacer::SetConfig(const SamplePacerConfig& config)
  {
    // The settings the pacer has change nothing
    m_impl->Pacer.SetSettings(ToPacerSettings(config));
  }


  SamplePacerSchedule SamplePacer::BeginFrame(const TickCount cpuStartTime) noexcept
  {
    const PC::FrameSchedule src = m_impl->Pacer.BeginFrame(FP::TickCount64(cpuStartTime.Ticks()));

    SamplePacerSchedule schedule;
    schedule.FrameId = src.FrameId;
    schedule.NextFrameStartTime = TickCount(src.NextFrameStartTime.Ticks());
    schedule.SwapInterval = src.SwapInterval;
    schedule.AnimationStep = TimeSpan(src.AnimationStep.Ticks());
    schedule.IntendedDisplayTime = TickCount(src.IntendedDisplayTime.Ticks());
    schedule.TargetFrameTime = TimeSpan(static_cast<int64_t>(src.TargetFrameTime.Ticks()));
    schedule.PreferredFrameTime = TimeSpan(static_cast<int64_t>(src.PreferredFrameTime.Ticks()));
    schedule.Change = ToSamplePacerChange(src.Change);
    return schedule;
  }


  void SamplePacer::EndFrame(const TickCount presentTime, const TimeSpan work) noexcept
  {
    m_impl->Pacer.EndFrame(FP::TickCount64(presentTime.Ticks()), FP::TimeSpan(std::max(work.Ticks(), int64_t{0})));
  }


  void SamplePacer::AddPresentFeedback(const uint64_t frameId, const TickCount displayTime, const std::optional<TickCount> presentTime) noexcept
  {
    const FP::TickCount64 shownTime(displayTime.Ticks());
    m_impl->Pacer.AddPresentFeedback(presentTime.has_value() ? PC::PresentFeedback::Shown(frameId, shownTime, FP::TickCount64(presentTime->Ticks()))
                                                             : PC::PresentFeedback::Shown(frameId, shownTime));
  }


  void SamplePacer::AddPresentNotShown(const uint64_t frameId) noexcept
  {
    m_impl->Pacer.AddPresentFeedback(PC::PresentFeedback::NotShown(frameId));
  }


  SamplePacerFeedbackState SamplePacer::GetFeedbackState() const noexcept
  {
    const PC::PresentFeedbackState src = m_impl->Pacer.FeedbackState();

    SamplePacerFeedbackState state;
    state.Used = src.Used;
    state.Refused = src.Refused;
    state.NotShown = src.NotShown;
    state.Missing = src.Missing;
    state.LateRefreshes = src.LateRefreshes;
    return state;
  }


  SamplePacerStatus SamplePacer::GetStatus() const noexcept
  {
    const PC::FrameWindowState window = m_impl->Pacer.FrameWindow();

    SamplePacerStatus status;
    status.SwapInterval = m_impl->Pacer.SwapInterval();
    status.PreferredSwapInterval = m_impl->Pacer.Settings().PreferredSwapIntervalAt(m_impl->Pacer.Refresh());
    status.Frames = window.Frames;
    status.LateFrames = window.LateFrames;
    status.AverageWork = TimeSpan(window.AverageWork.Ticks());
    status.WindowSpan = TimeSpan(window.Span.Ticks());
    status.WindowFull = window.Full;
    return status;
  }

#else

  struct SamplePacer::Impl
  {
  };


  bool SamplePacer::IsSupported() noexcept
  {
    return false;
  }


  SamplePacer::SamplePacer(const SamplePacerConfig& config)
  {
    FSL_PARAM_NOT_USED(config);
  }


  SamplePacer::~SamplePacer() = default;


  void SamplePacer::SetConfig(const SamplePacerConfig& config)
  {
    FSL_PARAM_NOT_USED(config);
  }


  SamplePacerSchedule SamplePacer::BeginFrame(const TickCount cpuStartTime) noexcept
  {
    FSL_PARAM_NOT_USED(cpuStartTime);
    return {};
  }


  void SamplePacer::EndFrame(const TickCount presentTime, const TimeSpan work) noexcept
  {
    FSL_PARAM_NOT_USED(presentTime);
    FSL_PARAM_NOT_USED(work);
  }


  void SamplePacer::AddPresentFeedback(const uint64_t frameId, const TickCount displayTime, const std::optional<TickCount> presentTime) noexcept
  {
    FSL_PARAM_NOT_USED(frameId);
    FSL_PARAM_NOT_USED(displayTime);
    FSL_PARAM_NOT_USED(presentTime);
  }


  void SamplePacer::AddPresentNotShown(const uint64_t frameId) noexcept
  {
    FSL_PARAM_NOT_USED(frameId);
  }


  SamplePacerFeedbackState SamplePacer::GetFeedbackState() const noexcept
  {
    return {};
  }


  SamplePacerStatus SamplePacer::GetStatus() const noexcept
  {
    return {};
  }

#endif
}
