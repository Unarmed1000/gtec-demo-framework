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
#include <FslDemoApp/Base/FrameInfo.hpp>
#include <FslDemoApp/Base/Service/AppInfo/IAppInfoService.hpp>
#include <FslDemoApp/Base/Service/Host/IHostInfo.hpp>
#include <FslDemoApp/Shared/Host/DemoHostFeature.hpp>
#include <FslDemoService/FramePacingMarker/Impl/FramePacingFrameRecordUtil.hpp>
#include <FslDemoService/FramePacingMarker/Impl/FramePacingMarkerService.hpp>
#include <FslDemoService/FramePacingMarker/Impl/FramePacingMarkerServiceOptionParser.hpp>
#include <FslDemoService/FramePacingMarker/Impl/FramePacingOverlay.hpp>
#include <FslDemoService/SystemStats/ISystemStatsService.hpp>
#include <FslDemoService/Trace/Control/ITraceServiceControl.hpp>
#include <FslDemoService/Trace/ITraceService.hpp>
#include <mb/framepacing/marker/Options.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! How often the load of the machine is written to the log
      constexpr TimeSpan LogSystemLoadInterval = TimeSpan::FromSeconds(1);
    }

    int32_t ClampModuleSize(const int32_t moduleSizePx) noexcept
    {
      return std::clamp(moduleSizePx, MB::FramePacing::Marker::Options::MinModuleSizePx, MB::FramePacing::Marker::Options::MaxModuleSizePx);
    }

    //! The sequence id as 32 hex digits (the way the mb-framepacing tools show a sequence id that is not printable text)
    std::string ToHexString(const FramePacingSequenceId& sequenceId)
    {
      constexpr std::string_view HexDigits("0123456789abcdef");
      std::string result;
      result.reserve(sequenceId.Bytes.size() * 2u);
      for (const uint8_t value : sequenceId.Bytes)
      {
        result.push_back(HexDigits[value >> 4u]);
        result.push_back(HexDigits[value & 0x0Fu]);
      }
      return result;
    }
  }


  FramePacingMarkerService::FramePacingMarkerService(const ServiceProvider& serviceProvider,
                                                     const std::shared_ptr<FramePacingMarkerServiceOptionParser>& optionParser)
    : ThreadLocalService(serviceProvider)
    , m_random(std::random_device{}())
    , m_enabled(optionParser->IsEnabled())
    , m_syncMarkerEnabled(optionParser->IsSyncMarkerEnabled())
    , m_moduleSizePx(ClampModuleSize(optionParser->GetModuleSizePx()))
    , m_captureHeightPx(std::max(optionParser->GetCaptureHeightPx(), 0))
    , m_nextRunId(optionParser->GetRunId())
  {
    if (optionParser->GetRunName().has_value())
    {
      m_pendingRun = PendingRun{optionParser->GetRunName().value(), optionParser->GetRunDuration()};
    }
    m_runId = m_nextRunId.has_value() ? m_nextRunId.value() : CreateRunId();

    // The frames are logged to the trace of the trace service, if it is on
    m_trace = serviceProvider.TryGet<ITraceService>();
    m_traceControl = serviceProvider.TryGet<ITraceServiceControl>();
    if (m_trace && m_traceControl && m_trace->IsEnabled())
    {
      RegisterLogColumns();
      // On the timeline of the trace: when the marker of a frame was drawn, and when its pacer means the frame to be shown
      const TraceTrack markerTrack = m_trace->RegisterTrack("Marker", TraceTrackKind::Sequential);
      m_trace->DeclareMark("marker drawn", markerTrack, m_logColumns.MarkerDraw, TraceLink::NoLink);
      const TraceTrack planTrack = m_trace->RegisterTrack("Pacer plan", TraceTrackKind::Sequential);
      m_trace->DeclareMark("intended display", planTrack, m_logColumns.IntendedDisplay, TraceLink::NoLink);
    }
    else
    {
      m_trace.reset();
      m_traceControl.reset();
    }
  }


  FramePacingMarkerService::~FramePacingMarkerService() = default;


  void FramePacingMarkerService::Link(const ServiceProvider& serviceProvider)
  {
    ThreadLocalService::Link(serviceProvider);
    if (m_trace)
    {
      // Only used by the log. A service can only reach the services of a higher priority, so this one is registered below the default priority
      m_appInfo = serviceProvider.TryGet<IAppInfoService>();
      m_hostInfo = serviceProvider.TryGet<IHostInfo>();
      m_systemStats = serviceProvider.TryGet<ISystemStatsService>();
    }
  }


  bool FramePacingMarkerService::IsEnabled() const noexcept
  {
    return m_enabled;
  }


  void FramePacingMarkerService::SetEnabled(const bool enabled) noexcept
  {
    m_enabled = enabled;
    if (!enabled)
    {
      // The next marker is drawn after it is enabled again
      m_lastMarker.reset();
    }
  }


  bool FramePacingMarkerService::IsSyncMarkerEnabled() const noexcept
  {
    return m_syncMarkerEnabled;
  }


  void FramePacingMarkerService::SetSyncMarkerEnabled(const bool enabled) noexcept
  {
    m_syncMarkerEnabled = enabled;
  }


  int32_t FramePacingMarkerService::GetModuleSizePx() const noexcept
  {
    return m_moduleSizePx;
  }


  void FramePacingMarkerService::SetModuleSizePx(const int32_t moduleSizePx) noexcept
  {
    m_moduleSizePx = ClampModuleSize(moduleSizePx);
  }


  int32_t FramePacingMarkerService::GetCaptureHeightPx() const noexcept
  {
    return m_captureHeightPx;
  }


  void FramePacingMarkerService::SetCaptureHeightPx(const int32_t captureHeightPx) noexcept
  {
    m_captureHeightPx = std::max(captureHeightPx, 0);
  }


  bool FramePacingMarkerService::BeginRun(const StringViewLite name, const TimeSpan duration)
  {
    if (!m_sequence.BeginRun(duration))
    {
      FSLLOG3_WARNING("FramePacing: a run is already active");
      return false;
    }
    // A explicitly requested run id is only used for the first run
    m_runId = m_nextRunId.has_value() ? m_nextRunId.value() : CreateRunId();
    m_nextRunId.reset();
    m_runName = std::string(std::string_view(name));
    // The start marker identifies the run by the sequence id, the name is only logged
    m_runSequenceId = CreateSequenceId();
    m_runStartTime = std::chrono::system_clock::now();
    m_enabled = true;
    // A command line run is superseded by any explicit run
    m_pendingRun.reset();
    FSLLOG3_INFO("FramePacing: run '{}' (id {}, sequence id {}) started", m_runName, m_runId, ToHexString(m_runSequenceId));
    if (m_trace)
    {
      m_trace->AddEvent("runStarted", fmt::format("runId={};sequenceId={};name={};durationTicks={};utcNanoseconds={}", m_runId,
                                                  ToHexString(m_runSequenceId), m_runName, duration.Ticks(),
                                                  std::chrono::duration_cast<std::chrono::nanoseconds>(m_runStartTime.time_since_epoch()).count()));
    }
    return true;
  }


  void FramePacingMarkerService::EndRun() noexcept
  {
    m_sequence.EndRun();
  }


  FramePacingRunState FramePacingMarkerService::GetRunState() const noexcept
  {
    return m_sequence.GetState();
  }


  uint32_t FramePacingMarkerService::GetRunId() const noexcept
  {
    return m_runId;
  }


  TimeSpan FramePacingMarkerService::GetRunDuration() const noexcept
  {
    return m_sequence.GetMeasureDuration();
  }


  TimeSpan FramePacingMarkerService::GetRunMeasuredTime() const noexcept
  {
    return m_sequence.GetMeasuredTime(m_timer.GetTimestamp());
  }


  void FramePacingMarkerService::BeginFrame(const FrameInfo& frameInfo, const TickCount cpuStartTime)
  {
    if (m_pendingRun.has_value())
    {
      const PendingRun pendingRun = m_pendingRun.value();
      BeginRun(StringViewLite(pendingRun.Name), pendingRun.Duration);
    }

    const FramePacingRunState oldState = m_sequence.GetState();
    m_frameKind = m_sequence.Advance(m_timer.GetTimestamp());
    if (oldState != FramePacingRunState::Idle && m_sequence.GetState() == FramePacingRunState::Idle)
    {
      FSLLOG3_INFO("FramePacing: run '{}' (id {}) completed", m_runName, m_runId);
      if (m_trace)
      {
        m_trace->AddEvent("runCompleted", fmt::format("runId={}", m_runId));
      }
    }

    m_frameIndex = m_frameCount;
    ++m_frameCount;
    // The animation time the app uses, TickCount is in 100ns ticks exactly like the marker format.
    m_frameAnimationTicks = frameInfo.Time.CurrentTickCount.Ticks();
    // A HighResolutionTimer timestamp, also in 100ns ticks
    m_frameCpuStartTicks = cpuStartTime.Ticks();
    // An app with its own frame pacer supplies the values of the frame during its draw
    m_frameSchedule.reset();
    m_hasFrame = true;

    if (m_trace)
    {
      if (!m_logFactsWritten)
      {
        // In the trace a frame lasts from where the host started on it to where the host was done with it. Not done before the
        // first frame, as the host adds its value after this service was created
        m_traceControl->SetFrameBounds(m_logColumns.HostCpuStart, m_trace->FindValue("hostSwapCompletedTicks"));
      }
      m_traceControl->BeginFrame(TraceFrameIndex(m_frameIndex), TraceRunId(m_runId));
      if (!m_logFactsWritten)
      {
        // Not done before the first frame, as the name of the app is not known when the service is created
        m_logFactsWritten = true;
        WriteLogFacts();
      }
      m_trace->SetInt64(m_logColumns.MarkerKind, static_cast<int64_t>(m_frameKind));
      m_trace->SetUInt64(m_logColumns.RunId, m_runId);
      m_trace->SetInt64(m_logColumns.RunState, static_cast<int64_t>(m_sequence.GetState()));
      m_trace->SetInt64(m_logColumns.AnimationTime, m_frameAnimationTicks);
      m_trace->SetInt64(m_logColumns.CpuStart, m_frameCpuStartTicks);
      m_trace->SetInt64(m_logColumns.HostCpuStart, m_frameCpuStartTicks);
      m_trace->SetInt64(m_logColumns.BeginFrame, m_timer.GetTimestamp().Ticks());
      m_trace->SetInt64(m_logColumns.HasSchedule, 0);
      m_trace->SetInt64(m_logColumns.MarkerDrawn, 0);
      WriteLogSystemLoad(m_timer.GetTimestamp());
    }
  }


  std::unique_ptr<IFramePacingOverlay> FramePacingMarkerService::CreateOverlay(const ServiceProvider& serviceProvider)
  {
    return FramePacingOverlay::TryCreate(serviceProvider);
  }


  bool FramePacingMarkerService::TryGetLastMarker(FramePacingMarkerInfo& rInfo) const noexcept
  {
    if (!m_enabled || !m_lastMarker.has_value())
    {
      return false;
    }
    rInfo = *m_lastMarker;
    return true;
  }


  void FramePacingMarkerService::SetFrameSchedule(const FramePacingFrameSchedule& schedule) noexcept
  {
    m_frameSchedule = schedule;
    if (m_trace)
    {
      // The values of the app replace the ones of the framework, as they do in the marker
      m_trace->SetInt64(m_logColumns.HasSchedule, 1);
      m_trace->SetInt64(m_logColumns.AnimationTime, schedule.AnimationTime.Ticks());
      if (schedule.CpuStartTime.has_value())
      {
        m_trace->SetInt64(m_logColumns.CpuStart, schedule.CpuStartTime->Ticks());
      }
      if (schedule.IntendedDisplayTime.has_value())
      {
        m_trace->SetInt64(m_logColumns.IntendedDisplay, schedule.IntendedDisplayTime->Ticks());
      }
      if (schedule.TargetFrameTime.has_value())
      {
        m_trace->SetInt64(m_logColumns.TargetFrameTime, schedule.TargetFrameTime->Ticks());
      }
      if (schedule.PreferredFrameTime.has_value())
      {
        m_trace->SetInt64(m_logColumns.PreferredFrameTime, schedule.PreferredFrameTime->Ticks());
      }
    }
  }


  bool FramePacingMarkerService::TryGetFrameRecord(FramePacingFrameRecord& rRecord) const noexcept
  {
    if (!m_enabled || !m_hasFrame)
    {
      return false;
    }
    rRecord.Kind = m_frameKind;
    rRecord.FrameIndex = m_frameIndex;
    rRecord.AnimationTicks = m_frameAnimationTicks;
    rRecord.CpuStartTicks = m_frameCpuStartTicks;
    // The framework has no frame pacer, so the pacing values are unknown unless the app supplied them
    rRecord.IntendedDisplayTicks = 0;
    rRecord.TargetFrameTicks = 0;
    rRecord.PreferredFrameTicks = 0;
    rRecord.Static = false;
    if (m_frameSchedule.has_value())
    {
      FramePacingFrameRecordUtil::ApplySchedule(rRecord, *m_frameSchedule);
    }
    rRecord.RunId = m_runId;
    // A frame with the animation time of the frame before it: nothing animated while that frame was on screen (the app is paused or
    // only animates on demand). The analysis then does not judge that step. The service only knows this in hindsight.
    rRecord.StaticBefore = m_lastMarker.has_value() && FramePacingFrameRecordUtil::IsStaticBefore(rRecord, *m_lastMarker);
    rRecord.RunStartTime = m_runStartTime;
    rRecord.RunSequenceId = m_runSequenceId;
    rRecord.SyncMarkerEnabled = m_syncMarkerEnabled;
    rRecord.ModuleSizePx = m_moduleSizePx;
    rRecord.CaptureHeightPx = m_captureHeightPx;
    return true;
  }


  void FramePacingMarkerService::SetLastMarker(const FramePacingMarkerInfo& markerInfo) noexcept
  {
    m_lastMarker = markerInfo;
    if (m_trace)
    {
      // What the marker that was just drawn carries, where it differs from what was known when the frame began
      m_trace->SetInt64At(TraceFrameIndex(markerInfo.FrameIndex), m_logColumns.MarkerDrawn, 1);
      m_trace->SetInt64At(TraceFrameIndex(markerInfo.FrameIndex), m_logColumns.MarkerDraw, m_timer.GetTimestamp().Ticks());
      m_trace->SetInt64At(TraceFrameIndex(markerInfo.FrameIndex), m_logColumns.MarkerKind, static_cast<int64_t>(markerInfo.Kind));
      if (markerInfo.CpuBusyTime.has_value())
      {
        m_trace->SetInt64At(TraceFrameIndex(markerInfo.FrameIndex), m_logColumns.MarkerCpuBusy, markerInfo.CpuBusyTime->Ticks());
      }
      m_trace->SetInt64At(TraceFrameIndex(markerInfo.FrameIndex), m_logColumns.MarkerStatic, markerInfo.Static ? 1 : 0);
      m_trace->SetInt64At(TraceFrameIndex(markerInfo.FrameIndex), m_logColumns.MarkerStaticBefore, markerInfo.StaticBefore ? 1 : 0);
      m_trace->SetInt64At(TraceFrameIndex(markerInfo.FrameIndex), m_logColumns.MarkerSync, markerInfo.SyncMarker ? 1 : 0);
      m_trace->SetInt64At(TraceFrameIndex(markerInfo.FrameIndex), m_logColumns.MarkerModuleSize, m_moduleSizePx);
    }
  }


  void FramePacingMarkerService::RegisterLogColumns()
  {
    if (!m_trace)
    {
      return;
    }
    ITraceService& rLog = *m_trace;
    m_logColumns.MarkerKind =
      rLog.RegisterValue("markerKind", TraceUnit::Code, "The kind of marker of the frame: 0 frame, 1 the start of a run, 2 the end of a run");
    m_logColumns.RunId = rLog.RegisterValue("runId", TraceUnit::Id, "The id of the current (or last) run, as the marker carries it");
    m_logColumns.RunState = rLog.RegisterValue("runState", TraceUnit::Code, "The state of the run: 0 idle, 1 starting, 2 measuring, 3 ending");
    m_logColumns.AnimationTime =
      rLog.RegisterValue("animationTimeTicks", TraceUnit::DurationTicks,
                         "The time the frame is animated for, as the marker carries it (the one of the app if it gave a schedule)");
    m_logColumns.CpuStart = rLog.RegisterValue(
      "cpuStartTicks", TraceUnit::Ticks, "When the CPU started on the frame, as the marker carries it (the one of the app if it gave a schedule)");
    m_logColumns.HostCpuStart = rLog.RegisterValue("hostCpuStartTicks", TraceUnit::Ticks, "When the host started the update of the frame");
    m_logColumns.BeginFrame =
      rLog.RegisterValue("beginFrameTicks", TraceUnit::Ticks,
                         "When the host told the service the frame begins: after the update and after the frame was prepared for drawing");
    m_logColumns.HasSchedule = rLog.RegisterValue("hasSchedule", TraceUnit::Flag, "1 if the app gave the pacing values of the frame");
    m_logColumns.IntendedDisplay =
      rLog.RegisterValue("intendedDisplayTicks", TraceUnit::Ticks, "When the frame pacer of the app intends the frame to be shown");
    m_logColumns.TargetFrameTime =
      rLog.RegisterValue("targetFrameTimeTicks", TraceUnit::DurationTicks, "The frame time the frame pacer of the app aims for");
    m_logColumns.PreferredFrameTime =
      rLog.RegisterValue("preferredFrameTimeTicks", TraceUnit::DurationTicks, "The frame time the app wants to run at");
    m_logColumns.MarkerDrawn = rLog.RegisterValue("markerDrawn", TraceUnit::Flag, "1 if the marker was drawn on the frame");
    m_logColumns.MarkerDraw = rLog.RegisterValue("markerDrawTicks", TraceUnit::Ticks, "When the marker was drawn, the last thing of the frame");
    m_logColumns.MarkerCpuBusy = rLog.RegisterValue("markerCpuBusyTicks", TraceUnit::DurationTicks,
                                                    "How long the CPU worked on the frame before the marker was drawn, as the marker carries it");
    m_logColumns.MarkerStatic = rLog.RegisterValue("markerStatic", TraceUnit::Flag, "The static after flag of the marker");
    m_logColumns.MarkerStaticBefore = rLog.RegisterValue("markerStaticBefore", TraceUnit::Flag, "The static before flag of the marker");
    m_logColumns.MarkerSync = rLog.RegisterValue("markerSyncMarker", TraceUnit::Flag, "1 if the sync marker was drawn as well");
    m_logColumns.MarkerModuleSize =
      rLog.RegisterValue("markerModuleSizePx", TraceUnit::Pixels, "The size of a module of the marker that was asked for");

    // The load of the machine, written about once a second and empty in between. The CPU times are the counters of the operating system:
    // the load between two rows that have them is the difference.
    m_logColumns.SystemIdle = rLog.RegisterValue("systemIdleTicks", TraceUnit::DurationTicks,
                                                 "How long the CPUs of the system were idle since it started, summed over the CPUs");
    m_logColumns.SystemKernel = rLog.RegisterValue("systemKernelTicks", TraceUnit::DurationTicks,
                                                   "How long the CPUs of the system ran kernel code since it started, idle time not included");
    m_logColumns.SystemUser =
      rLog.RegisterValue("systemUserTicks", TraceUnit::DurationTicks, "How long the CPUs of the system ran user code since it started");
    m_logColumns.ProcessKernel =
      rLog.RegisterValue("processKernelTicks", TraceUnit::DurationTicks, "How long this process ran kernel code since it started");
    m_logColumns.ProcessUser =
      rLog.RegisterValue("processUserTicks", TraceUnit::DurationTicks, "How long this process ran user code since it started");
    m_logColumns.ProcessGpuUsage = rLog.RegisterValue("processGpuUsageMilliPercent", TraceUnit::Count,
                                                      "The load of the busiest GPU engine this process uses in thousandths of a percent");
    m_logColumns.ProcessGpuDedicated =
      rLog.RegisterValue("processGpuDedicatedBytes", TraceUnit::Count, "The memory of the GPU this process uses in bytes");
    m_logColumns.ProcessGpuShared =
      rLog.RegisterValue("processGpuSharedBytes", TraceUnit::Count, "The system memory the GPU uses for this process in bytes");
  }


  void FramePacingMarkerService::WriteLogSystemLoad(const TickCount currentTime)
  {
    if (!m_trace || !m_systemStats ||
        (m_logSystemSampleTime.Ticks() != 0 && (currentTime - m_logSystemSampleTime) < LocalConfig::LogSystemLoadInterval))
    {
      return;
    }
    m_logSystemSampleTime = currentTime;

    // How busy the machine is decides what a frame loop can do, so the log carries it. The counters are written as they are.
    SystemCpuTimes cpuTimes;
    if (m_systemStats->TryGetCpuTimes(cpuTimes))
    {
      m_trace->SetUInt64(m_logColumns.SystemIdle, cpuTimes.SystemIdleTicks);
      m_trace->SetUInt64(m_logColumns.SystemKernel, cpuTimes.SystemKernelTicks);
      m_trace->SetUInt64(m_logColumns.SystemUser, cpuTimes.SystemUserTicks);
      m_trace->SetUInt64(m_logColumns.ProcessKernel, cpuTimes.ProcessKernelTicks);
      m_trace->SetUInt64(m_logColumns.ProcessUser, cpuTimes.ProcessUserTicks);
    }
    GpuUsageRecord gpuUsage;
    if (m_systemStats->TryGetApplicationGpuUsage(gpuUsage))
    {
      // The system reports a percentage, it is written in thousandths of a percent
      m_trace->SetInt64(m_logColumns.ProcessGpuUsage, std::llround(static_cast<double>(gpuUsage.UsagePercentage) * 1000.0));
    }
    GpuMemoryUsageRecord gpuMemoryUsage;
    if (m_systemStats->TryGetApplicationGpuMemoryUsage(gpuMemoryUsage))
    {
      m_trace->SetUInt64(m_logColumns.ProcessGpuDedicated, gpuMemoryUsage.DedicatedBytes);
      m_trace->SetUInt64(m_logColumns.ProcessGpuShared, gpuMemoryUsage.SharedBytes);
    }
  }


  void FramePacingMarkerService::WriteLogFacts()
  {
    if (!m_trace)
    {
      return;
    }
    ITraceService& rLog = *m_trace;
    // The clock every time of the log is on, and the same moment as a wall clock time so a log can be related to other recordings
    rLog.SetFact("clock", "HighResolutionTimer, 100 nanosecond ticks");
    rLog.SetFact("clockNativeFrequency", fmt::format("{}", m_timer.GetNativeTickFrequency()));
    {
      const auto utcNow = std::chrono::system_clock::now();
      const TickCount clockNow = m_timer.GetTimestamp();
      rLog.SetFact("utcNanoseconds", fmt::format("{}", std::chrono::duration_cast<std::chrono::nanoseconds>(utcNow.time_since_epoch()).count()));
      rLog.SetFact("utcClockTicks", fmt::format("{}", clockNow.Ticks()));
    }
    if (m_appInfo)
    {
      rLog.SetFact("app", std::string_view(m_appInfo->GetAppName()));
      rLog.SetFact("debugBuild", m_appInfo->IsDebugBuild() ? "1" : "0");
    }
    if (m_hostInfo)
    {
      const DemoHostFeature api = m_hostInfo->GetActiveAPI();
      rLog.SetFact("api", DemoHostFeatureName::ToString(api.Name));
      rLog.SetFact("apiVersion", fmt::format("{:#x}", api.Version));
    }
    rLog.SetFact("marker.enabled", m_enabled ? "1" : "0");
    rLog.SetFact("marker.syncMarker", m_syncMarkerEnabled ? "1" : "0");
    rLog.SetFact("marker.moduleSizePx", fmt::format("{}", m_moduleSizePx));
    rLog.SetFact("marker.captureHeightPx", fmt::format("{}", m_captureHeightPx));
  }


  uint32_t FramePacingMarkerService::CreateRunId()
  {
    std::uniform_int_distribution<uint32_t> distribution(1u, std::numeric_limits<uint32_t>::max());
    return distribution(m_random);
  }


  FramePacingSequenceId FramePacingMarkerService::CreateSequenceId()
  {
    std::uniform_int_distribution<uint32_t> distribution(0u, std::numeric_limits<uint8_t>::max());
    FramePacingSequenceId sequenceId;
    for (uint8_t& rValue : sequenceId.Bytes)
    {
      rValue = static_cast<uint8_t>(distribution(m_random));
    }
    return sequenceId;
  }
}
