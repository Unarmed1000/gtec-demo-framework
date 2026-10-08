#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_FRAMEPACINGMARKERSERVICE_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_FRAMEPACINGMARKERSERVICE_HPP
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

#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslDemoService/FramePacingMarker/Control/IFramePacingMarkerServiceControl.hpp>
#include <FslDemoService/FramePacingMarker/IFramePacingMarkerService.hpp>
#include <FslDemoService/FramePacingMarker/Impl/FramePacingSequence.hpp>
#include <FslDemoService/FramePacingMarker/Impl/IFramePacingFrameSource.hpp>
#include <FslDemoService/Trace/TraceTypes.hpp>
#include <FslService/Impl/ServiceType/Local/ThreadLocalService.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>

namespace Fsl
{
  class FramePacingMarkerServiceOptionParser;
  class IAppInfoService;
  class IHostInfo;
  class ISystemStatsService;
  class ITraceService;
  class ITraceServiceControl;

  class FramePacingMarkerService final
    : public ThreadLocalService
    , public IFramePacingMarkerService
    , public IFramePacingMarkerServiceControl
    , public IFramePacingFrameSource
  {
    struct PendingRun
    {
      std::string Name;
      TimeSpan Duration;
    };

    HighResolutionTimer m_timer;
    std::mt19937 m_random;
    FramePacingSequence m_sequence;

    bool m_enabled;
    bool m_syncMarkerEnabled;
    int32_t m_moduleSizePx;
    int32_t m_captureHeightPx;

    //! A run requested on the command line, it is started at the first frame
    std::optional<PendingRun> m_pendingRun;
    std::optional<uint32_t> m_nextRunId;

    uint32_t m_runId{0};
    std::string m_runName;
    FramePacingSequenceId m_runSequenceId;
    std::chrono::system_clock::time_point m_runStartTime;

    //! The number of frames that were started so far
    uint64_t m_frameCount{0};
    bool m_hasFrame{false};
    FramePacingMarkerKind m_frameKind{FramePacingMarkerKind::Frame};
    uint64_t m_frameIndex{0};
    int64_t m_frameAnimationTicks{0};
    int64_t m_frameCpuStartTicks{0};
    //! The pacing values the app supplied for the frame being drawn (empty: the framework's values are reported)
    std::optional<FramePacingFrameSchedule> m_frameSchedule;

    //! Every value the last drawn marker carried (empty until the overlay drew a marker)
    std::optional<FramePacingMarkerInfo> m_lastMarker;

    //! The values of a frame the service records itself
    struct LogColumns
    {
      TraceValue MarkerKind;
      TraceValue RunId;
      TraceValue RunState;
      TraceValue AnimationTime;
      TraceValue CpuStart;
      TraceValue HostCpuStart;
      TraceValue BeginFrame;
      TraceValue HasSchedule;
      TraceValue IntendedDisplay;
      TraceValue TargetFrameTime;
      TraceValue PreferredFrameTime;
      TraceValue MarkerDrawn;
      TraceValue MarkerDraw;
      TraceValue MarkerCpuBusy;
      TraceValue MarkerStatic;
      TraceValue MarkerStaticBefore;
      TraceValue MarkerSync;
      TraceValue MarkerModuleSize;
      TraceValue SystemIdle;
      TraceValue SystemKernel;
      TraceValue SystemUser;
      TraceValue ProcessKernel;
      TraceValue ProcessUser;
      TraceValue ProcessGpuUsage;
      TraceValue ProcessGpuDedicated;
      TraceValue ProcessGpuShared;
    };

    //! Where the frames are logged: the trace of the trace service (both null: the trace is off and the frames are not logged)
    std::shared_ptr<ITraceService> m_trace;
    //! The service begins the frames of the trace, so a frame of the trace is the frame its marker carries
    std::shared_ptr<ITraceServiceControl> m_traceControl;
    LogColumns m_logColumns;
    bool m_logFactsWritten{false};
    std::shared_ptr<IAppInfoService> m_appInfo;
    std::shared_ptr<IHostInfo> m_hostInfo;
    //! For the load of the machine in the log (null if the platform has no system stats service)
    std::shared_ptr<ISystemStatsService> m_systemStats;
    //! When the load of the machine was last written to the log (zero = never)
    TickCount m_logSystemSampleTime;

  public:
    FramePacingMarkerService(const ServiceProvider& serviceProvider, const std::shared_ptr<FramePacingMarkerServiceOptionParser>& optionParser);
    ~FramePacingMarkerService() final;

    void Link(const ServiceProvider& serviceProvider) final;

    // From IFramePacingMarkerService
    [[nodiscard]] bool IsEnabled() const noexcept final;
    void SetEnabled(const bool enabled) noexcept final;
    [[nodiscard]] bool IsSyncMarkerEnabled() const noexcept final;
    void SetSyncMarkerEnabled(const bool enabled) noexcept final;
    [[nodiscard]] int32_t GetModuleSizePx() const noexcept final;
    void SetModuleSizePx(const int32_t moduleSizePx) noexcept final;
    [[nodiscard]] int32_t GetCaptureHeightPx() const noexcept final;
    void SetCaptureHeightPx(const int32_t captureHeightPx) noexcept final;
    bool BeginRun(const StringViewLite name, const TimeSpan duration) final;
    void EndRun() noexcept final;
    [[nodiscard]] FramePacingRunState GetRunState() const noexcept final;
    [[nodiscard]] uint32_t GetRunId() const noexcept final;
    [[nodiscard]] TimeSpan GetRunDuration() const noexcept final;
    [[nodiscard]] TimeSpan GetRunMeasuredTime() const noexcept final;
    bool TryGetLastMarker(FramePacingMarkerInfo& rInfo) const noexcept final;
    void SetFrameSchedule(const FramePacingFrameSchedule& schedule) noexcept final;

    // From IFramePacingMarkerServiceControl
    void BeginFrame(const FrameInfo& frameInfo, const TickCount cpuStartTime) final;
    std::unique_ptr<IFramePacingOverlay> CreateOverlay(const ServiceProvider& serviceProvider) final;

    // From IFramePacingFrameSource
    bool TryGetFrameRecord(FramePacingFrameRecord& rRecord) const noexcept final;
    void SetLastMarker(const FramePacingMarkerInfo& markerInfo) noexcept final;

  private:
    void RegisterLogColumns();
    void WriteLogFacts();
    void WriteLogSystemLoad(const TickCount currentTime);
    uint32_t CreateRunId();
    FramePacingSequenceId CreateSequenceId();
  };
}

#endif
