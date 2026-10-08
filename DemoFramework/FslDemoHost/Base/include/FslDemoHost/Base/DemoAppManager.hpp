#ifndef FSLDEMOHOST_BASE_DEMOAPPMANAGER_HPP
#define FSLDEMOHOST_BASE_DEMOAPPMANAGER_HPP
/****************************************************************************************************************************************************
 * Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslBase/Math/Point2.hpp>
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslDemoApp/Base/AppDrawResult.hpp>
#include <FslDemoApp/Base/DemoAppConfig.hpp>
#include <FslDemoApp/Base/DemoAppStatsFlags.hpp>
#include <FslDemoApp/Base/Host/DemoAppSetup.hpp>
#include <FslDemoApp/Base/IDemoApp.hpp>
#include <FslDemoApp/Base/TimeStepMode.hpp>
#include <FslDemoHost/Base/DemoAppManagerProcessResult.hpp>
#include <FslDemoHost/Base/DemoAppTiming.hpp>
#include <FslDemoHost/Base/DemoState.hpp>
#include <FslDemoHost/Base/LogStatsMode.hpp>
#include <FslDemoService/Trace/TraceTypes.hpp>
#include <memory>
#include <utility>

namespace Fsl
{
  class DemoAppManagerEventListener;
  class DemoAppProfilerOverlay;
  class IDemoAppControlEx;
  class ICpuStatsService;
  class ISystemStatsService;
  class IFramePacingOverlay;
  class IFramePacingMarkerServiceControl;
  class IGraphicsServiceControl;
  class INativeWindow;
  class IProfilerService;
  class IProfilerServiceControl;
  class ITraceService;
  struct TimeSpan;

  class DemoAppManager
  {
    struct AppRecord
    {
      std::shared_ptr<IDemoApp> DemoApp;
      uint32_t FrameIndex{0};

      AppRecord() = default;
      explicit AppRecord(std::shared_ptr<IDemoApp> demoApp)
        : DemoApp(std::move(demoApp))
      {
      }
    };
    struct OnDemandRendering
    {
      uint16_t LastOnDemandFrameInterval{0};
      uint64_t WaitTimeInTicks{0};
      uint64_t TimeInTicks{0};
      uint64_t SkipCount{0};
      bool ForceRenderNextFrame{true};
    };

    struct CachedState
    {
      TimeStepMode CachedTimeStepMode{TimeStepMode::Normal};
    };

    struct Stats
    {
      TickCount TimeBeforeUpdate{0u};
      TickCount TimeAfterUpdate{0u};
      TickCount TimeAfterDraw{0u};
      TickCount LastFrameSwapCompletedTime{0};
    };

    std::unique_ptr<DemoAppProfilerOverlay> m_demoAppProfilerOverlay;
    //! Null if the frame pacing service is unavailable
    std::shared_ptr<IFramePacingOverlay> m_framePacingOverlay;
    //! The trace service (null: the trace is off)
    std::shared_ptr<ITraceService> m_trace;
    TraceZone m_traceZoneUpdate;
    TraceZone m_traceZonePrepareDraw;
    TraceZone m_traceZoneDraw;
    TraceZone m_traceZoneAppSwap;
    //! The stages of the update and of the draw, which are inside the zones above
    TraceZone m_traceZonePreUpdate;
    TraceZone m_traceZoneFixedUpdate;
    TraceZone m_traceZoneAppUpdate;
    TraceZone m_traceZonePostUpdate;
    TraceZone m_traceZoneResolve;
    TraceZone m_traceZoneBeginDraw;
    TraceZone m_traceZoneAppDraw;
    TraceZone m_traceZoneMarkerDraw;
    TraceZone m_traceZoneEndDraw;
    TraceZone m_traceZoneProfilerDraw;
    std::shared_ptr<IFramePacingMarkerServiceControl> m_framePacingMarkerServiceControl;

    //! The values of a frame the host adds to the trace
    struct FramePacingLogColumns
    {
      TraceValue UpdateEnd;
      TraceValue DrawEnd;
      TraceValue SwapCall;
      TraceValue SwapReturn;
      TraceValue SwapCompleted;
      TraceValue FrameSlot;
      TraceValue FrameworkTime;
      TraceValue FrameworkStep;
      TraceValue DisplayVSync;
      TraceValue DisplayRefreshPeriod;
      TraceValue DisplayRefreshPeriodNs;
      TraceValue DisplayVSyncFlags;
      TraceValue DisplayVBlankInterval;
      TraceValue DisplayVBlankOffPeriod;
    };

    //! Null if the frame pacing service is unavailable
    FramePacingLogColumns m_framePacingLogColumns;
    //! The frame of the log the host is working on (valid if m_framePacingLogHasFrame)
    TraceFrameIndex m_framePacingLogFrameIndex;
    bool m_framePacingLogHasFrame{false};
    PxExtent2D m_framePacingLogExtentPx;
    //! The window the display of the log is read from
    std::weak_ptr<INativeWindow> m_framePacingLogWindow;
    //! The refresh interval of the display that was last written to the log, in nanoseconds (negative: none was written)
    int64_t m_framePacingLogRefreshIntervalNs{-1};
    //! The answers about variable refresh that were written to the log last (packed, -1: nothing was written yet)
    int32_t m_framePacingLogVariableRefresh{-1};
    std::shared_ptr<IDemoAppControlEx> m_demoAppControl;
    std::shared_ptr<IGraphicsServiceControl> m_graphicsService;
    std::shared_ptr<IProfilerServiceControl> m_profilerServiceControl;
    std::shared_ptr<IProfilerService> m_profilerService;
    std::shared_ptr<ICpuStatsService> m_cpuStatsService;
    //! For the GPU stats (null if the platform has no system stats service)
    std::shared_ptr<ISystemStatsService> m_systemStatsService;
    std::shared_ptr<DemoAppManagerEventListener> m_eventListener;
    DemoAppSetup m_demoAppSetup;
    DemoAppConfig m_demoAppConfig;
    DemoState m_state;
    CachedState m_cachedState;
    bool m_hasExitRequest{false};
    HighResolutionTimer m_timer;
    DemoAppTiming m_appTiming;
    DemoTime m_currentDemoTimeDraw;
    Stats m_stats;
    OnDemandRendering m_onDemandRendering;
    LogStatsMode m_logStatsMode;
    DemoAppStatsFlags m_logStatsFlags;
    bool m_enableStats;
    bool m_useFirewall;

    AppRecord m_record;

  public:
    DemoAppManager(DemoAppSetup demoAppSetup, const DemoAppConfig& demoAppConfig, const bool enableStats, const LogStatsMode logStatsMode,
                   const DemoAppStatsFlags& logStatsFlags, const bool enableFirewall, const bool enableContentMonitor,
                   const TimeSpan& forcedUpdateTime, const bool renderSystemOverlay);
    virtual ~DemoAppManager();

    [[nodiscard]] uint32_t GetFrameIndex() const
    {
      return m_record.FrameIndex;
    }

    void Suspend(const bool bSuspend);

    [[nodiscard]] DemoState GetState() const;

    DemoAppManagerProcessResult Process(const DemoWindowMetrics& windowMetrics, const bool isConsoleBasedApp);
    AppDrawResult TryDraw();
    void OnDrawSkipped();

    //! @note Only called if the app is controlling the swap buffers.
    AppDrawResult TryAppSwapBuffers();

    void OnActivate();
    void OnDeactivate();
    //! @brief Should be called after a buffer swap has been completed
    void OnFrameSwapCompleted();

    //! @brief The host swapped the frame (or asked the app to do it): when the call was made and when it returned.
    void OnSwapBuffers(const TickCount callTime, const TickCount returnTime) noexcept;
    void OnDemandDrawSkipped();
    void ProcessDone();

    void RequestExit();
    [[nodiscard]] bool HasExitRequest() const;
    [[nodiscard]] bool HasRestartRequest() const;
    int CloseApp();

  private:
    void CacheState();
    void UpdateAppTimers();
    DemoAppManagerProcessResult ProcessOnDemandRendering();

    //! @brief Manage exit requests
    //! @return true if exit should occur right away
    bool ManageExitRequests(const bool bCheckExternalOnly);
    //! @brief manage the app state
    void ManageAppState(const DemoWindowMetrics& windowMetrics, const bool isConsoleBasedApp);
    void ResetTimer();

    void DoShutdownAppNow();
  };
}

#endif
