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
#include <Shared/FramePacing/SamplePacerTierChoice.hpp>
#include <algorithm>
#include <cmath>
#include <concepts>
#include <memory>
#include <utility>

// The pacer library is only available on the platforms that support the frame pacing marker library
#ifdef FSL_ENABLE_MB_FRAMEPACING
#include <mb/framepacing/core/time/NanosecondTickCount.hpp>
#include <mb/framepacing/core/time/NanosecondTimeDuration.hpp>
#include <mb/framepacing/core/time/NanosecondTimeSpan.hpp>
#include <mb/framepacing/pacer/PacerAim.hpp>
#include <mb/framepacing/pacer/PacerSettings.hpp>
#include <mb/framepacing/pacer/RefreshPeriod.hpp>
#include <mb/framepacing/pacer/capability/PacerCapabilities.hpp>
#include <mb/framepacing/pacer/capability/PacerCapability.hpp>
#include <mb/framepacing/pacer/capability/PacerMajorTier.hpp>
#include <mb/framepacing/pacer/capability/PacerRating.hpp>
#include <mb/framepacing/pacer/capability/PacerTier.hpp>
#include <mb/framepacing/pacer/capability/PacerTierText.hpp>
#include <mb/framepacing/pacer/capability/PacerTierUtil.hpp>
#include <mb/framepacing/pacer/display/DisplayErrorState.hpp>
#include <mb/framepacing/pacer/display/DisplayReport.hpp>
#include <mb/framepacing/pacer/frame/FrameSchedule.hpp>
#include <mb/framepacing/pacer/frame/FrameStartPlan.hpp>
#include <mb/framepacing/pacer/frame/GpuWaitReport.hpp>
#include <mb/framepacing/pacer/frame/GpuWorkReport.hpp>
#include <mb/framepacing/pacer/frame/PresentPlan.hpp>
#include <mb/framepacing/pacer/frame/PresentReport.hpp>
#include <mb/framepacing/pacer/frame/PresentWaitReport.hpp>
#include <mb/framepacing/pacer/frame/SystemWaitKind.hpp>
#include <mb/framepacing/pacer/frame/SystemWaitReport.hpp>
#include <mb/framepacing/pacer/frame/VBlankReading.hpp>
#include <mb/framepacing/pacer/rule/FrameWindowState.hpp>
#include <mb/framepacing/pacer/rule/SwapIntervalChange.hpp>
#include <mb/framepacing/pacer/tier/TierPacer.hpp>
#endif

namespace Fsl
{
  std::string_view SamplePacer::GetTierLogName(const SamplePacerTier tier, const bool timedPresent) noexcept
  {
    if (timedPresent)
    {
      switch (tier)
      {
      case SamplePacerTier::VBlankWaitForPresent:
        return "timedVBlankWaitForPresent";
      case SamplePacerTier::VBlankPeriodOnly:
        return "timedVBlankPeriodOnly";
      case SamplePacerTier::TimerWaitForPresent:
        return "timedTimerWaitForPresent";
      case SamplePacerTier::TimerPeriodOnly:
        break;
      }
      return "timedTimerPeriodOnly";
    }
    switch (tier)
    {
    case SamplePacerTier::VBlankWaitForPresent:
      return "vblankWaitForPresent";
    case SamplePacerTier::VBlankPeriodOnly:
      return "vblankPeriodOnly";
    case SamplePacerTier::TimerWaitForPresent:
      return "timerWaitForPresent";
    case SamplePacerTier::TimerPeriodOnly:
      break;
    }
    return "timerPeriodOnly";
  }


#ifdef FSL_ENABLE_MB_FRAMEPACING
  namespace
  {
    namespace FP = MB::FramePacing;
    namespace PC = MB::FramePacing::Pacer;

    constexpr SamplePacerTier ToSampleTier(const PC::PacerTier tier) noexcept
    {
      // A tier where the display places the frame is the tier of the sample where the loop does: that the display places it is
      // said next to it (IsTimedTier)
      switch (tier)
      {
      case PC::PacerTier::TimedSkipVBlankWaitForPresent:
      case PC::PacerTier::TimedVBlankWaitForPresent:
      case PC::PacerTier::VBlankWaitForPresent:
        return SamplePacerTier::VBlankWaitForPresent;
      case PC::PacerTier::TimedSkipVBlankPeriodOnly:
      case PC::PacerTier::TimedVBlankPeriodOnly:
      case PC::PacerTier::VBlankPeriodOnly:
        return SamplePacerTier::VBlankPeriodOnly;
      case PC::PacerTier::TimedSkipTimerWaitForPresent:
      case PC::PacerTier::TimedTimerWaitForPresent:
      case PC::PacerTier::TimerWaitForPresent:
        return SamplePacerTier::TimerWaitForPresent;
      case PC::PacerTier::TimedSkipTimerPeriodOnly:
      case PC::PacerTier::TimedTimerPeriodOnly:
      case PC::PacerTier::TimerPeriodOnly:
        break;
      }
      return SamplePacerTier::TimerPeriodOnly;
    }

    //! True for the tiers where the display places the frame (a present at a time): every tier but the ones of the last major tier,
    //! where the frame loop does
    constexpr bool IsTimedTier(const PC::PacerTier tier) noexcept
    {
      return PC::PacerTierUtil::MajorOf(tier) != PC::PacerMajorTier::LoopPlaces;
    }

    constexpr PC::PacerTier ToLibraryTier(const SamplePacerTier tier, const bool timedPresent = false) noexcept
    {
      if (timedPresent)
      {
        switch (tier)
        {
        case SamplePacerTier::VBlankWaitForPresent:
          return PC::PacerTier::TimedVBlankWaitForPresent;
        case SamplePacerTier::VBlankPeriodOnly:
          return PC::PacerTier::TimedVBlankPeriodOnly;
        case SamplePacerTier::TimerWaitForPresent:
          return PC::PacerTier::TimedTimerWaitForPresent;
        case SamplePacerTier::TimerPeriodOnly:
          break;
        }
        return PC::PacerTier::TimedTimerPeriodOnly;
      }
      switch (tier)
      {
      case SamplePacerTier::VBlankWaitForPresent:
        return PC::PacerTier::VBlankWaitForPresent;
      case SamplePacerTier::VBlankPeriodOnly:
        return PC::PacerTier::VBlankPeriodOnly;
      case SamplePacerTier::TimerWaitForPresent:
        return PC::PacerTier::TimerWaitForPresent;
      case SamplePacerTier::TimerPeriodOnly:
        break;
      }
      return PC::PacerTier::TimerPeriodOnly;
    }

    namespace LocalConfig
    {
      // The refresh periods the pacer accepts: 100 microseconds (10 kHz) to 1 second (1 Hz)
      constexpr int64_t MinRefreshPeriodNs = 100000;
      constexpr int64_t MaxRefreshPeriodNs = 1000000000;
    }

    // The times of the framework are ticks of 100 ns, the ones of the library are nanoseconds, both on the steady clock. A time of
    // the framework is a time of the library exactly. The other way a point in time is cut to its tick and a length of time is
    // rounded to the nearest one, as NanosecondTickCountUtil and NanosecondTimeSpanUtil do it.
    constexpr FP::NanosecondTickCount ToLibrary(const TickCount time) noexcept
    {
      return FP::NanosecondTickCount(time.Ticks() * FP::NanosecondTickCount::NanosecondsPerTick);
    }

    constexpr FP::NanosecondTimeSpan ToLibrary(const TimeSpan span) noexcept
    {
      return FP::NanosecondTimeSpan(span.Ticks() * FP::NanosecondTimeSpan::NanosecondsPerTick);
    }

    constexpr TickCount ToFramework(const FP::NanosecondTickCount time) noexcept
    {
      const int64_t nanoseconds = time.Nanoseconds();
      const int64_t ticks = nanoseconds / FP::NanosecondTickCount::NanosecondsPerTick;
      return TickCount(((nanoseconds % FP::NanosecondTickCount::NanosecondsPerTick) < 0) ? (ticks - 1) : ticks);
    }

    constexpr TimeSpan ToFramework(const FP::NanosecondTimeSpan span) noexcept
    {
      const int64_t nanoseconds = span.Nanoseconds();
      const int64_t half = FP::NanosecondTimeSpan::NanosecondsPerTick / 2;
      return TimeSpan((nanoseconds >= 0 ? (nanoseconds + half) : (nanoseconds - half)) / FP::NanosecondTimeSpan::NanosecondsPerTick);
    }

    constexpr TimeSpan ToFramework(const FP::NanosecondTimeDuration duration) noexcept
    {
      return ToFramework(FP::NanosecondTimeSpan(duration.Nanoseconds()));
    }

    // The same value in the nanosecond type of the framework: nothing is rounded
    constexpr NanosecondTickCount ToFrameworkNanoseconds(const FP::NanosecondTickCount time) noexcept
    {
      return NanosecondTickCount(time.Nanoseconds());
    }

    constexpr NanosecondTimeSpan ToFrameworkNanoseconds(const FP::NanosecondTimeSpan span) noexcept
    {
      return NanosecondTimeSpan(span.Nanoseconds());
    }

    constexpr NanosecondTimeSpan ToFrameworkNanoseconds(const FP::NanosecondTimeDuration duration) noexcept
    {
      return NanosecondTimeSpan(duration.Nanoseconds());
    }

    //! What a app can do, as the library takes it
    PC::PacerCapabilities ToLibrary(const SamplePacerCapabilities& capabilities) noexcept
    {
      PC::PacerCapability bits = PC::PacerCapability::NoCapabilities;
      if (capabilities.PresentSwapIntervalMax > 0u)
      {
        bits = bits | PC::PacerCapability::PresentSwapInterval;
      }
      if (capabilities.PresentAfterDuration)
      {
        bits = bits | PC::PacerCapability::PresentAfterDuration;
      }
      if (capabilities.VBlankTimes)
      {
        bits = bits | PC::PacerCapability::VBlankTimes;
      }
      if (capabilities.WaitForPresent)
      {
        bits = bits | PC::PacerCapability::WaitForPresent;
      }
      if (capabilities.DisplayTimes)
      {
        bits = bits | PC::PacerCapability::DisplayTimes;
      }
      if (capabilities.WaitForGpuWork)
      {
        bits = bits | PC::PacerCapability::WaitForGpuWork;
      }
      return PC::PacerCapabilities(bits, std::max(capabilities.PresentSwapIntervalMax, 1u));
    }

    //! What of it the pacer is to use with the given config: the vertical blank times and the wait for a present where the kind is
    //! one that uses them, and the present that takes a time where the config asks for it.
    PC::PacerCapabilities ToActiveCapabilities(const SamplePacerConfig& config) noexcept
    {
      SamplePacerCapabilities active = config.Capabilities;
      active.VBlankTimes = config.Capabilities.VBlankTimes && SamplePacerTierChoiceUtil::IsVBlankKind(config.Kind);
      active.WaitForPresent = config.Capabilities.WaitForPresent && SamplePacerTierChoiceUtil::IsPresentWaitKind(config.Kind);
      active.PresentAfterDuration = config.Capabilities.PresentAfterDuration && config.TimedPresent;
      // The display times are used where the run gives them to the pacer
      active.DisplayTimes = config.Capabilities.DisplayTimes && config.PresentFeedback;
      // The wait for the GPU's work where the config asks for it. The pacer does not ask for it next to a wait for a present.
      active.WaitForGpuWork = config.Capabilities.WaitForGpuWork && config.GpuWait;
      return ToLibrary(active);
    }

    PC::PacerSettings ToPacerSettings(const SamplePacerConfig& config) noexcept
    {
      // The refresh period as it was given, in nanoseconds (a period in whole 100ns ticks would drift)
      const int64_t refreshPeriodNs =
        config.RefreshPeriod.TotalNanoseconds() > 0 ? config.RefreshPeriod.TotalNanoseconds() : SamplePacerConfig().RefreshPeriod.TotalNanoseconds();
      PC::PacerSettings settings(PC::RefreshPeriod::FromNanosecondTimeSpan(
        FP::NanosecondTimeSpan(std::clamp(refreshPeriodNs, LocalConfig::MinRefreshPeriodNs, LocalConfig::MaxRefreshPeriodNs))));
      if (config.TargetFps > 0u)
      {
        settings.SetPreferredFrameRate(config.TargetFps);
      }
      settings.SetAutoSwapInterval(config.Adaptive);
      settings.SetAim(config.Aim == SamplePacerAim::LowLatency ? PC::PacerAim::LowLatency : PC::PacerAim::Smoothness);
      // Zero: the pacer picks
      settings.SetWaitingPresents(std::min(config.WaitingPresents, PC::PacerSettings::MaxWaitingPresents));
      settings.SetMaxFramesInFlight(std::clamp(config.MaxFramesInFlight, 1u, PC::PacerSettings::MaxMaxFramesInFlight));
      settings.SetStartupPauseRefreshes(std::min(config.StartupPauseRefreshes, PC::PacerSettings::MaxStartupPauseRefreshes));
      settings.SetReadyPlacePercent(std::min(config.ReadyPlacePercent, PC::PacerSettings::MaxReadyPlacePercent));
      settings.SetSwapChainImages(std::min(config.SwapChainImages, PC::PacerSettings::MaxSwapChainImages));
      settings.SetSystemHoldsLoop(config.SystemHoldsLoop);
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

    //! True if the two differ in more than the kind and what the app has: in the settings of the pacer
    bool HasOtherSettings(const SamplePacerConfig& config, const SamplePacerConfig& current) noexcept
    {
      SamplePacerConfig compared = config;
      compared.Kind = current.Kind;
      compared.Capabilities = current.Capabilities;
      compared.TimedPresent = current.TimedPresent;
      compared.GpuWait = current.GpuWait;
      // The display times are part of the set the pacer uses
      compared.PresentFeedback = current.PresentFeedback;
      return compared != current;
    }
  }

  // The framework and the library both count time in 100ns ticks, and a HighResolutionTimer timestamp is the steady clock the pacer needs
  struct SamplePacer::Impl
  {
    SamplePacerConfig Config;
    PC::TierPacer Pacer;

    // The pacer is told what the app has, and the kind is what of that it uses. It is made with what it uses, so it starts as a
    // pacer that never had more (taking a capability away is not the same as never having had it: a pacer that stops waiting for
    // presents makes no pause after its start).
    explicit Impl(const SamplePacerConfig& config)
      : Config(config)
      , Pacer(ToPacerSettings(config), ToActiveCapabilities(config))
    {
      Pacer.SetCapabilities(ToLibrary(config.Capabilities));
      Pacer.SetActiveCapabilities(ToActiveCapabilities(config));
    }
  };


  bool SamplePacer::IsSupported() noexcept
  {
    return true;
  }


  SamplePacerRating SamplePacer::Rate(const SamplePacerCapabilities& capabilities) noexcept
  {
    // The rating is the library's: the best tier the set reaches, and if the display side can hold a frame
    const PC::PacerRating rating = PC::PacerTierUtil::Rate(ToLibrary(capabilities));
    return {ToSampleTier(rating.Tier), IsTimedTier(rating.Tier), rating.DisplaySideHolds};
  }


  std::string_view SamplePacer::GetTierNumber(const SamplePacerTier tier, const bool timedPresent) noexcept
  {
    return PC::PacerTierText::NumberOf(ToLibraryTier(tier, timedPresent));
  }


  // The words for the tiers are the pacer library's, as the definition of a tier is
  std::string_view SamplePacer::GetTierName(const SamplePacerTier tier, const bool timedPresent) noexcept
  {
    return PC::PacerTierText::NameOf(ToLibraryTier(tier, timedPresent));
  }


  std::string_view SamplePacer::GetTierDescription(const SamplePacerTier tier) noexcept
  {
    return PC::PacerTierText::DescriptionOf(ToLibraryTier(tier));
  }


  std::string_view SamplePacer::GetTierShortDescription(const SamplePacerTier tier, const bool timedPresent) noexcept
  {
    return PC::PacerTierText::ShortDescriptionOf(ToLibraryTier(tier, timedPresent));
  }


  uint32_t SamplePacer::GetTierShortDescriptionMaxLength() noexcept
  {
    return PC::PacerTierText::ShortDescriptionMaxLength;
  }


  SamplePacer::SamplePacer(const SamplePacerConfig& config)
    : m_impl(std::make_unique<Impl>(config))
  {
  }


  SamplePacer::~SamplePacer() = default;


  void SamplePacer::SetConfig(const SamplePacerConfig& config)
  {
    if (config.Kind != m_impl->Config.Kind || config.Capabilities != m_impl->Config.Capabilities ||
        config.TimedPresent != m_impl->Config.TimedPresent || config.GpuWait != m_impl->Config.GpuWait ||
        config.PresentFeedback != m_impl->Config.PresentFeedback)
    {
      // The same pacer with another set to use: nothing starts again. What the app has, then what of it is to be used: the pacer
      // leaves out what is not there.
      m_impl->Pacer.SetCapabilities(ToLibrary(config.Capabilities));
      m_impl->Pacer.SetActiveCapabilities(ToActiveCapabilities(config));
    }
    if (HasOtherSettings(config, m_impl->Config))
    {
      m_impl->Pacer.SetSettings(ToPacerSettings(config));
    }
    m_impl->Config = config;
  }


  SamplePacerTier SamplePacer::GetTier() const noexcept
  {
    return ToSampleTier(m_impl->Pacer.WorkingTier());
  }


  bool SamplePacer::IsTimedPresentInUse() const noexcept
  {
    // The pacer plans the time where the app has such a present and the config asks for it (ToActiveCapabilities)
    return m_impl->Config.TimedPresent && m_impl->Config.Capabilities.PresentAfterDuration;
  }


  void SamplePacer::ForgetPresents() noexcept
  {
    m_impl->Pacer.ForgetPresents();
  }


  TimeSpan SamplePacer::GetCpuBusyAt(const TickCount now) const noexcept
  {
    return ToFramework(m_impl->Pacer.CpuBusyAt(ToLibrary(now)));
  }


  SamplePacerFrameStartPlan SamplePacer::PlanFrame(const TickCount now) const noexcept
  {
    const PC::FrameStartPlan src = m_impl->Pacer.PlanFrame(ToLibrary(now));

    SamplePacerFrameStartPlan plan;
    if (src.WaitsForPresent())
    {
      plan.WaitForPresentFrameId = src.WaitForPresentFrameId;
      plan.WaitForPresentTimeout = ToFramework(src.WaitForPresentTimeout);
    }
    if (src.WaitsForGpuWork())
    {
      plan.WaitForGpuWorkFrameId = src.WaitForGpuWorkFrameId;
      plan.WaitForGpuWorkTimeout = ToFramework(src.WaitForGpuWorkTimeout);
    }
    if (src.WaitsForStartTime())
    {
      plan.StartTime = ToFramework(src.StartTime);
    }
    return plan;
  }


  void SamplePacer::AddPresentWait(const SamplePacerPresentWaitReport& report) noexcept
  {
    PC::PresentWaitReport dst;
    dst.FrameId = report.FrameId;
    dst.BeginTime = ToLibrary(report.BeginTime);
    dst.EndTime = ToLibrary(report.EndTime);
    dst.Shown = report.Shown;
    m_impl->Pacer.AddPresentWait(dst);
  }


  void SamplePacer::AddGpuWait(const SamplePacerGpuWaitReport& report) noexcept
  {
    PC::GpuWaitReport dst;
    dst.FrameId = report.FrameId;
    dst.BeginTime = ToLibrary(report.BeginTime);
    dst.EndTime = ToLibrary(report.EndTime);
    dst.Done = report.Done;
    m_impl->Pacer.AddGpuWait(dst);
  }


  uint64_t SamplePacer::GetGpuWaitTimeouts() const noexcept
  {
    return m_impl->Pacer.GpuWaitTimeouts();
  }


  SamplePacerSchedule SamplePacer::BeginFrame(const TickCount cpuStartTime) noexcept
  {
    const PC::FrameSchedule src = m_impl->Pacer.BeginFrame(ToLibrary(cpuStartTime));

    SamplePacerSchedule schedule;
    schedule.FrameId = src.FrameId;
    schedule.NextFrameStartTime = ToFramework(src.NextFrameStartTime);
    schedule.SwapInterval = src.SwapInterval;
    schedule.AnimationStep = ToFrameworkNanoseconds(src.AnimationStep);
    schedule.IntendedDisplayTime = ToFrameworkNanoseconds(src.IntendedDisplayTime);
    schedule.TargetFrameTime = ToFrameworkNanoseconds(src.TargetFrameTime);
    schedule.PreferredFrameTime = ToFrameworkNanoseconds(src.PreferredFrameTime);
    schedule.Change = ToSamplePacerChange(src.Change);
    return schedule;
  }


  SamplePacerPresentPlan SamplePacer::EndFrame(const TickCount workDoneTime) noexcept
  {
    const PC::PresentPlan src = m_impl->Pacer.EndFrame(ToLibrary(workDoneTime));

    SamplePacerPresentPlan plan;
    plan.FrameId = src.FrameId;
    if (src.WaitsForPresentTime())
    {
      plan.PresentTime = ToFramework(src.PresentTime);
    }
    plan.SwapInterval = src.SwapInterval;
    plan.MinimumDuration = ToFrameworkNanoseconds(src.MinimumDuration);
    plan.CpuBusy = ToFramework(src.CpuBusy);
    return plan;
  }


  void SamplePacer::AddPresent(const SamplePacerPresentReport& report) noexcept
  {
    PC::PresentReport dst;
    dst.FrameId = report.FrameId;
    dst.CallTime = ToLibrary(report.CallTime);
    dst.ReturnTime = ToLibrary(report.ReturnTime);
    dst.Accepted = report.Accepted;
    m_impl->Pacer.AddPresent(dst);
  }


  void SamplePacer::AddGpuWork(const SamplePacerGpuWorkReport& report) noexcept
  {
    const FP::NanosecondTimeDuration duration(ToLibrary(TimeSpan(std::max(report.Duration.Ticks(), int64_t{0}))));
    if (report.BeginTime.has_value() && report.EndTime.has_value())
    {
      m_impl->Pacer.AddGpuWork(PC::GpuWorkReport::Times(report.FrameId, ToLibrary(*report.BeginTime), ToLibrary(*report.EndTime)));
    }
    else if (report.EndTime.has_value())
    {
      m_impl->Pacer.AddGpuWork(PC::GpuWorkReport::EndAndDuration(report.FrameId, ToLibrary(*report.EndTime), duration));
    }
    else
    {
      m_impl->Pacer.AddGpuWork(PC::GpuWorkReport::OfDuration(report.FrameId, duration));
    }
  }


  TimeSpan SamplePacer::GetGpuTime() const noexcept
  {
    return ToFramework(m_impl->Pacer.GpuTime());
  }


  uint64_t SamplePacer::GetStartupPauses() const noexcept
  {
    return m_impl->Pacer.StartupPauses();
  }


  uint64_t SamplePacer::GetRefreshesBehindClock() const noexcept
  {
    return m_impl->Pacer.RefreshesBehindClock();
  }


  uint64_t SamplePacer::GetPresentWaitTimeouts() const noexcept
  {
    return m_impl->Pacer.PresentWaitTimeouts();
  }


  bool SamplePacer::IsPresentWaitStopped() const noexcept
  {
    return m_impl->Pacer.PresentWaitsStopped();
  }


  void SamplePacer::AddVBlank(const NanosecondTickCount vblankTime, const NanosecondTimeSpan period, const TickCount readTime) noexcept
  {
    // The nanoseconds of the window system go in as they are
    PC::VBlankReading reading;
    reading.VBlankTime = FP::NanosecondTickCount(vblankTime.TotalNanoseconds());
    reading.Period = FP::NanosecondTimeDuration(FP::NanosecondTimeSpan(std::max(period.TotalNanoseconds(), int64_t{0})));
    reading.ReadTime = ToLibrary(readTime);
    m_impl->Pacer.AddVBlank(reading);
  }


  bool SamplePacer::HasVBlankReading() const noexcept
  {
    return m_impl->Pacer.HasVBlankReading();
  }


  uint64_t SamplePacer::GetVBlankJumps() const noexcept
  {
    return m_impl->Pacer.VBlankJumps();
  }


  uint64_t SamplePacer::GetShownLaterByWaits() const noexcept
  {
    return m_impl->Pacer.ShownLaterByWaits();
  }


  TimeSpan SamplePacer::GetReadyPlaceNow() const noexcept
  {
    return ToFramework(m_impl->Pacer.ReadyPlaceNow());
  }


  uint64_t SamplePacer::GetReadyPlaceTries() const noexcept
  {
    return m_impl->Pacer.ReadyPlaceTries();
  }


  uint64_t SamplePacer::GetReadyPlaceTriesTakenBack() const noexcept
  {
    return m_impl->Pacer.ReadyPlaceTriesTakenBack();
  }


  void SamplePacer::AddSystemWait(const SamplePacerSystemWait kind, const TickCount beginTime, const TickCount endTime) noexcept
  {
    PC::SystemWaitReport report;
    report.Kind = kind == SamplePacerSystemWait::FrameSlot ? PC::SystemWaitKind::FrameSlot : PC::SystemWaitKind::Acquire;
    report.BeginTime = ToLibrary(beginTime);
    report.EndTime = ToLibrary(endTime);
    m_impl->Pacer.AddSystemWait(report);
  }


  uint64_t SamplePacer::GetSystemHeldFrames() const noexcept
  {
    return m_impl->Pacer.SystemHeldFrames();
  }


  uint64_t SamplePacer::GetFrameSlotHeldFrames() const noexcept
  {
    return m_impl->Pacer.FrameSlotHeldFrames();
  }


  uint64_t SamplePacer::GetDisplayHeldRefreshes() const noexcept
  {
    return m_impl->Pacer.DisplayHeldRefreshes();
  }


  void SamplePacer::AddDisplayTime(const uint64_t frameId, const TickCount displayTime) noexcept
  {
    // A display report, which the pacer counts the animation error from
    PC::DisplayReport report;
    report.FrameId = frameId;
    report.DisplayTime = ToLibrary(displayTime);
    report.Shown = true;
    m_impl->Pacer.AddDisplayReport(report);
  }


  SamplePacerDisplayErrors SamplePacer::GetDisplayErrors() const noexcept
  {
    const PC::DisplayErrorState src = m_impl->Pacer.DisplayErrors();

    SamplePacerDisplayErrors errors;
    errors.Reports = src.Reports;
    errors.Refused = src.Refused;
    errors.JudgedFrames = src.JudgedFrames;
    errors.ErrorFrames = src.ErrorFrames;
    errors.OffTargetFrames = src.OffTargetFrames;
    errors.LateFrames = src.LateFrames;
    errors.StartToDisplayFrames = src.StartToDisplayFrames;
    errors.StartToDisplayTotal = ToFrameworkNanoseconds(src.StartToDisplayTotal);
    errors.StartToDisplayLongest = ToFrameworkNanoseconds(src.StartToDisplayLongest);
    errors.RecentJudgedFrames = src.RecentJudgedFrames;
    errors.RecentErrorFrames = src.RecentErrorFrames;
    errors.RecentOffTargetFrames = src.RecentOffTargetFrames;
    errors.RecentLateFrames = src.RecentLateFrames;
    return errors;
  }


  SamplePacerStatus SamplePacer::GetStatus() const noexcept
  {
    const PC::TierPacer& pacer = m_impl->Pacer;
    const PC::FrameWindowState window = pacer.FrameWindow();

    SamplePacerStatus status;
    status.SwapInterval = pacer.SwapInterval();
    status.PreferredSwapInterval = pacer.Settings().PreferredSwapIntervalAt(pacer.Refresh());
    status.Frames = window.Frames;
    status.LateFrames = window.LateFrames;
    status.StartsAhead = ToFramework(window.StartsAhead);
    status.AverageWork = ToFramework(window.AverageWork);
    status.WindowSpan = ToFramework(window.Span);
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


  SamplePacerRating SamplePacer::Rate(const SamplePacerCapabilities& capabilities) noexcept
  {
    FSL_PARAM_NOT_USED(capabilities);
    return {};
  }


  std::string_view SamplePacer::GetTierNumber(const SamplePacerTier tier, const bool timedPresent) noexcept
  {
    FSL_PARAM_NOT_USED(timedPresent);
    FSL_PARAM_NOT_USED(tier);
    return {};
  }


  // Without the pacer library there are no tiers
  std::string_view SamplePacer::GetTierName(const SamplePacerTier tier, const bool timedPresent) noexcept
  {
    FSL_PARAM_NOT_USED(timedPresent);
    FSL_PARAM_NOT_USED(tier);
    return {};
  }


  std::string_view SamplePacer::GetTierDescription(const SamplePacerTier tier) noexcept
  {
    FSL_PARAM_NOT_USED(tier);
    return {};
  }


  std::string_view SamplePacer::GetTierShortDescription(const SamplePacerTier tier, const bool timedPresent) noexcept
  {
    FSL_PARAM_NOT_USED(timedPresent);
    FSL_PARAM_NOT_USED(tier);
    return {};
  }


  uint32_t SamplePacer::GetTierShortDescriptionMaxLength() noexcept
  {
    return 0;
  }


  void SamplePacer::AddGpuWork(const SamplePacerGpuWorkReport& report) noexcept
  {
    FSL_PARAM_NOT_USED(report);
  }


  TimeSpan SamplePacer::GetGpuTime() const noexcept
  {
    return {};
  }


  uint64_t SamplePacer::GetStartupPauses() const noexcept
  {
    return 0;
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


  SamplePacerTier SamplePacer::GetTier() const noexcept
  {
    return SamplePacerTier::TimerPeriodOnly;
  }


  bool SamplePacer::IsTimedPresentInUse() const noexcept
  {
    return false;
  }


  void SamplePacer::ForgetPresents() noexcept
  {
  }


  TimeSpan SamplePacer::GetCpuBusyAt(const TickCount now) const noexcept
  {
    FSL_PARAM_NOT_USED(now);
    return {};
  }


  SamplePacerFrameStartPlan SamplePacer::PlanFrame(const TickCount now) const noexcept
  {
    FSL_PARAM_NOT_USED(now);
    return {};
  }


  void SamplePacer::AddPresentWait(const SamplePacerPresentWaitReport& report) noexcept
  {
    FSL_PARAM_NOT_USED(report);
  }


  void SamplePacer::AddGpuWait(const SamplePacerGpuWaitReport& report) noexcept
  {
    FSL_PARAM_NOT_USED(report);
  }


  uint64_t SamplePacer::GetGpuWaitTimeouts() const noexcept
  {
    return 0;
  }


  SamplePacerPresentPlan SamplePacer::EndFrame(const TickCount workDoneTime) noexcept
  {
    FSL_PARAM_NOT_USED(workDoneTime);
    return {};
  }


  void SamplePacer::AddPresent(const SamplePacerPresentReport& report) noexcept
  {
    FSL_PARAM_NOT_USED(report);
  }


  uint64_t SamplePacer::GetRefreshesBehindClock() const noexcept
  {
    return 0;
  }


  uint64_t SamplePacer::GetPresentWaitTimeouts() const noexcept
  {
    return 0;
  }


  bool SamplePacer::IsPresentWaitStopped() const noexcept
  {
    return false;
  }


  void SamplePacer::AddVBlank(const NanosecondTickCount /*vblankTime*/, const NanosecondTimeSpan /*period*/, const TickCount /*readTime*/) noexcept
  {
  }


  bool SamplePacer::HasVBlankReading() const noexcept
  {
    return false;
  }


  uint64_t SamplePacer::GetVBlankJumps() const noexcept
  {
    return 0;
  }


  uint64_t SamplePacer::GetShownLaterByWaits() const noexcept
  {
    return 0;
  }


  TimeSpan SamplePacer::GetReadyPlaceNow() const noexcept
  {
    return {};
  }


  uint64_t SamplePacer::GetReadyPlaceTries() const noexcept
  {
    return 0;
  }


  uint64_t SamplePacer::GetReadyPlaceTriesTakenBack() const noexcept
  {
    return 0;
  }


  void SamplePacer::AddSystemWait(const SamplePacerSystemWait /*kind*/, const TickCount /*beginTime*/, const TickCount /*endTime*/) noexcept
  {
  }


  uint64_t SamplePacer::GetSystemHeldFrames() const noexcept
  {
    return 0;
  }


  uint64_t SamplePacer::GetFrameSlotHeldFrames() const noexcept
  {
    return 0;
  }


  uint64_t SamplePacer::GetDisplayHeldRefreshes() const noexcept
  {
    return 0;
  }


  void SamplePacer::AddDisplayTime(const uint64_t frameId, const TickCount displayTime) noexcept
  {
    FSL_PARAM_NOT_USED(frameId);
    FSL_PARAM_NOT_USED(displayTime);
  }


  SamplePacerDisplayErrors SamplePacer::GetDisplayErrors() const noexcept
  {
    return {};
  }


  SamplePacerStatus SamplePacer::GetStatus() const noexcept
  {
    return {};
  }

#endif
}
