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

#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/Log3Core.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Time/NanosecondTickCountUtil.hpp>
#include <FslBase/Time/NanosecondTimeSpanUtil.hpp>
#include <FslBase/Time/TimeSpanUtil.hpp>
#include <FslDemoApp/Base/DemoAppFirewall.hpp>
#include <FslDemoApp/Base/FrameInfo.hpp>
#include <FslDemoApp/Base/Host/IDemoAppFactory.hpp>
#include <FslDemoApp/Base/Overlay/DemoAppProfilerOverlay.hpp>
#include <FslDemoApp/Base/Service/ContentMonitor/IContentMonitor.hpp>
#include <FslDemoApp/Base/Service/Events/IEventService.hpp>
#include <FslDemoHost/Base/DemoAppManager.hpp>
#include <FslDemoHost/Base/DemoAppManagerEventListener.hpp>
#include <FslDemoHost/Base/Service/AppInfo/IAppInfoControlService.hpp>
#include <FslDemoHost/Base/Service/DemoAppControl/IDemoAppControlEx.hpp>
#include <FslDemoHost/Base/Service/Profiler/IProfilerServiceControl.hpp>
#include <FslDemoHost/Base/Service/WindowHost/IWindowHostInfo.hpp>
#include <FslDemoService/CpuStats/ICpuStatsService.hpp>
#include <FslDemoService/FramePacingMarker/Control/IFramePacingMarkerServiceControl.hpp>
#include <FslDemoService/FramePacingMarker/Control/IFramePacingOverlay.hpp>
#include <FslDemoService/Graphics/Control/IGraphicsServiceControl.hpp>
#include <FslDemoService/Profiler/IProfilerService.hpp>
#include <FslDemoService/SystemStats/ISystemStatsService.hpp>
#include <FslDemoService/Trace/ITraceService.hpp>
#include <FslDemoService/Trace/ScopedTraceZone.hpp>
#include <FslNativeWindow/Base/INativeWindow.hpp>
#include <FslNativeWindow/Base/NativeWindowDisplayInfo.hpp>
#include <FslNativeWindow/Base/NativeWindowTimingSupport.hpp>
#include <FslNativeWindow/Base/NativeWindowVariableRefreshInfo.hpp>
#include <FslService/Consumer/ServiceProvider.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <cassert>
#include <memory>
#include <utility>

namespace Fsl
{
  namespace
  {
    constexpr inline bool CheckRestartFlags(const CustomDemoAppConfigRestartFlags restartFlags, const DemoWindowMetrics& newWindowMetrics,
                                            const DemoWindowMetrics& oldWindowMetrics)
    {
      return (CustomDemoAppConfigRestartFlagsUtil::IsFlagged(restartFlags, CustomDemoAppConfigRestartFlags::Resize) &&
              newWindowMetrics.ExtentPx != oldWindowMetrics.ExtentPx) ||
             (CustomDemoAppConfigRestartFlagsUtil::IsFlagged(restartFlags, CustomDemoAppConfigRestartFlags::DpiChange) &&
              !newWindowMetrics.IsEqualDpi(oldWindowMetrics));
    }
  }

  DemoAppManager::DemoAppManager(DemoAppSetup demoAppSetup, const DemoAppConfig& demoAppConfig, const bool enableStats,
                                 const LogStatsMode logStatsMode, const DemoAppStatsFlags& logStatsFlags, const bool enableFirewall,
                                 const bool enableContentMonitor, const TimeSpan& forcedUpdateTime, const bool renderSystemOverlay)
    : m_eventListener(std::make_shared<DemoAppManagerEventListener>())
    , m_demoAppSetup(std::move(demoAppSetup))
    , m_demoAppConfig(demoAppConfig)
    , m_state(DemoState::Running)
    , m_appTiming(m_timer.GetTimestamp(), forcedUpdateTime)
    , m_logStatsMode(logStatsMode)
    , m_logStatsFlags(logStatsFlags)
    , m_enableStats(enableStats)
    , m_useFirewall(enableFirewall)
  {
    if (renderSystemOverlay)
    {
      m_demoAppProfilerOverlay = std::make_unique<DemoAppProfilerOverlay>(demoAppConfig.DemoServiceProvider, logStatsFlags);
    }
    // The frame pacing service is only registered on platforms that support the marker
    m_framePacingMarkerServiceControl = m_demoAppConfig.DemoServiceProvider.TryGet<IFramePacingMarkerServiceControl>();
    if (m_framePacingMarkerServiceControl && renderSystemOverlay)
    {
      m_framePacingOverlay = m_framePacingMarkerServiceControl->CreateOverlay(m_demoAppConfig.DemoServiceProvider);
    }
    m_trace = m_demoAppConfig.DemoServiceProvider.TryGet<ITraceService>();
    if (m_trace && m_trace->IsEnabled())
    {
      m_traceZoneUpdate = m_trace->RegisterZone("Update");
      m_traceZonePrepareDraw = m_trace->RegisterZone("Prepare draw");
      m_traceZoneDraw = m_trace->RegisterZone("Draw");
      m_traceZoneAppSwap = m_trace->RegisterZone("App swap");
      m_traceZonePreUpdate = m_trace->RegisterZone("PreUpdate");
      m_traceZoneFixedUpdate = m_trace->RegisterZone("FixedUpdate");
      m_traceZoneAppUpdate = m_trace->RegisterZone("App update");
      m_traceZonePostUpdate = m_trace->RegisterZone("PostUpdate");
      m_traceZoneResolve = m_trace->RegisterZone("Resolve");
      m_traceZoneBeginDraw = m_trace->RegisterZone("BeginDraw");
      m_traceZoneAppDraw = m_trace->RegisterZone("App draw");
      m_traceZoneMarkerDraw = m_trace->RegisterZone("Marker draw");
      m_traceZoneEndDraw = m_trace->RegisterZone("EndDraw");
      m_traceZoneProfilerDraw = m_trace->RegisterZone("Profiler draw");
    }
    else
    {
      m_trace.reset();
    }
    if (m_trace)
    {
      // What the host knows about a frame, for the trace
      ITraceService& rLog = *m_trace;
      m_framePacingLogColumns.UpdateEnd = rLog.RegisterValue("hostUpdateEndTicks", TraceUnit::Ticks, "When the update of the app was done");
      m_framePacingLogColumns.DrawEnd = rLog.RegisterValue("hostDrawEndTicks", TraceUnit::Ticks, "When the draw of the app was done");
      m_framePacingLogColumns.SwapCall =
        rLog.RegisterValue("hostSwapCallTicks", TraceUnit::Ticks, "When the host started to swap the frame (or asked the app to present it)");
      m_framePacingLogColumns.SwapReturn = rLog.RegisterValue("hostSwapReturnTicks", TraceUnit::Ticks, "When the swap returned");
      m_framePacingLogColumns.SwapCompleted =
        rLog.RegisterValue("hostSwapCompletedTicks", TraceUnit::Ticks, "When the host was done with the frame, after the swap");
      m_framePacingLogColumns.FrameSlot =
        rLog.RegisterValue("hostFrameSlot", TraceUnit::Id, "The frame slot of the render loop the frame used (the frames in flight)");
      m_framePacingLogColumns.FrameworkTime =
        rLog.RegisterValue("frameworkTimeTicks", TraceUnit::DurationTicks, "The time of the framework the frame was updated and drawn for");
      m_framePacingLogColumns.FrameworkStep =
        rLog.RegisterValue("frameworkStepTicks", TraceUnit::DurationTicks, "The time step of the framework from the frame before");
      m_framePacingLogColumns.DisplayVSync =
        rLog.RegisterValue("displayVSyncTicks", TraceUnit::Ticks,
                           "The time of a recent vertical blank of the display as the window system reported it when the frame began "
                           "(empty if the platform does not report it)");
      m_framePacingLogColumns.DisplayRefreshPeriod =
        rLog.RegisterValue("displayRefreshPeriodTicks", TraceUnit::DurationTicks,
                           "The time between two refreshes of the display as the window system measured it, read with displayVSyncTicks "
                           "and rounded to the nearest tick");
      m_framePacingLogColumns.DisplayRefreshPeriodNs =
        rLog.RegisterValue("displayRefreshPeriodNs", TraceUnit::Nanoseconds,
                           "The time between two refreshes of the display as the window system measured it or has it for the mode of the "
                           "display, in nanoseconds");
      m_framePacingLogColumns.DisplayVSyncFlags =
        rLog.RegisterValue("displayVSyncFlags", TraceUnit::Code,
                           "What the window system says about how displayVSyncTicks was obtained (NativeWindowVSyncTimeFlags), zero where "
                           "it says nothing: 1 the frame was shown in sync with the display, 2 a time of the display hardware, 4 the "
                           "hardware signalled the frame was shown, 8 zero copy. Wayland sets them from the kind flags of presentation-time");
      m_framePacingLogColumns.DisplayVBlankInterval =
        rLog.RegisterValue("displayVBlankIntervalMilliPeriods", TraceUnit::Count,
                           "The median time between two refreshes of the display as the window system measured it, in thousandths of the "
                           "refresh period of its mode: 1000 is a display that refreshes at the rate of its mode, more is a display that "
                           "refreshes slower (variable refresh that follows the frames). Empty if the platform does not measure it");
      m_framePacingLogColumns.DisplayVBlankOffPeriod =
        rLog.RegisterValue("displayVBlankOffPeriodPerMille", TraceUnit::Count,
                           "The share of the last refreshes of the display that did not come one refresh period of its mode after the "
                           "one before, in thousandths, read with displayVBlankIntervalMilliPeriods");
    }
    m_demoAppControl = m_demoAppConfig.DemoServiceProvider.Get<IDemoAppControlEx>();
    m_graphicsService = m_demoAppConfig.DemoServiceProvider.TryGet<IGraphicsServiceControl>();
    m_profilerServiceControl = m_demoAppConfig.DemoServiceProvider.Get<IProfilerServiceControl>();
    m_profilerService = m_demoAppConfig.DemoServiceProvider.Get<IProfilerService>();
    m_cpuStatsService = m_demoAppConfig.DemoServiceProvider.TryGet<ICpuStatsService>();
    m_systemStatsService = m_demoAppConfig.DemoServiceProvider.TryGet<ISystemStatsService>();
    const auto appInfo = m_demoAppConfig.DemoServiceProvider.Get<IAppInfoControlService>();
    appInfo->SetAppName(StringViewLite(m_demoAppSetup.ApplicationName));

    m_demoAppControl->SetRenderLoopMaxFramesInFlight(m_demoAppSetup.CustomAppConfig.MaxFramesInFlight);

    if (enableContentMonitor)
    {
      const std::shared_ptr<IContentMonitor> contentMonitor = m_demoAppConfig.DemoServiceProvider.Get<IContentMonitor>();
      contentMonitor->Enable(true);
    }

    const std::shared_ptr<IEventService> eventsService = m_demoAppConfig.DemoServiceProvider.Get<IEventService>();
    eventsService->Register(m_eventListener);

    // if (!demoAppControl)
    //  throw std::invalid_argument("demoAppControl can not be null");

    m_demoAppControl->RequestUpdateTimerReset();
  }


  DemoAppManager::~DemoAppManager()
  {
    try
    {
      DoShutdownAppNow();
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_ERROR("DoShutdownAppNow failed: {}", ex.what());
      std::terminate();
    }
  }


  void DemoAppManager::Suspend(const bool bSuspend)
  {
    if (bSuspend)
    {
      m_state = DemoState::Suspended;
      // Ensure that the app is released when we enter suspended state
      DoShutdownAppNow();
    }
    else
    {
      m_state = DemoState::Running;
    }
  }


  DemoState DemoAppManager::GetState() const
  {
    return m_state;
  }


  DemoAppManagerProcessResult DemoAppManager::Process(const DemoWindowMetrics& windowMetrics, const bool isConsoleBasedApp)
  {
    if (ManageExitRequests(true))
    {
      return DemoAppManagerProcessResult(DemoAppManagerProcessResult::Command::SkipDraw);
    }

    if (m_state == DemoState::Suspended)
    {
      return DemoAppManagerProcessResult(DemoAppManagerProcessResult::Command::Draw);
    }

    ManageAppState(windowMetrics, isConsoleBasedApp);

    if (ManageExitRequests(false))
    {
      return DemoAppManagerProcessResult(DemoAppManagerProcessResult::Command::SkipDraw);
    }

    m_record.DemoApp->_Begin();

    // Detect metrics changes
    if (windowMetrics != m_demoAppConfig.WindowMetrics)
    {
      m_demoAppConfig.UpdateWindowMetrics(windowMetrics);

      if (m_graphicsService)
      {
        m_graphicsService->SetWindowMetrics(m_demoAppConfig.WindowMetrics);
      }
      m_record.DemoApp->_ConfigurationChanged(windowMetrics);
    }

    // Check if the update timer should be reset or not
    assert(m_demoAppControl);
    if (m_demoAppControl->HasUpdateTimerResetRequest())
    {
      m_demoAppControl->ClearUpdateTimerResetRequest();
      ResetTimer();
    }

    {
      if (m_cachedState.CachedTimeStepMode == TimeStepMode::Step)
      {
        m_demoAppControl->SetTimeStepMode(TimeStepMode::Paused);
      }
      const DemoTime currentUpdateTime = m_appTiming.GetUpdateTime();

      m_stats.TimeBeforeUpdate = m_timer.GetTimestamp();
      {
        const ScopedTraceZone traceZone(m_trace.get(), m_traceZoneUpdate);
        {
          const ScopedTraceZone traceZoneStage(m_trace.get(), m_traceZonePreUpdate);
          m_record.DemoApp->_PreUpdate(currentUpdateTime);
        }

        {    // Run all missing fixed updates
          std::optional<DemoTime> fixedTime = m_appTiming.TryFixedUpdate();
          while (fixedTime.has_value())
          {
            const ScopedTraceZone traceZoneStage(m_trace.get(), m_traceZoneFixedUpdate);
            m_record.DemoApp->_FixedUpdate(fixedTime.value());
            fixedTime = m_appTiming.TryFixedUpdate();
          }
        }

        if (m_graphicsService)
        {
          m_graphicsService->PreUpdate();
        }

        {
          const ScopedTraceZone traceZoneStage(m_trace.get(), m_traceZoneAppUpdate);
          m_record.DemoApp->_Update(currentUpdateTime);
        }
        {
          const ScopedTraceZone traceZoneStage(m_trace.get(), m_traceZonePostUpdate);
          m_record.DemoApp->_PostUpdate(currentUpdateTime);
        }
        {
          const ScopedTraceZone traceZoneStage(m_trace.get(), m_traceZoneResolve);
          m_record.DemoApp->_Resolve(currentUpdateTime);
        }
      }
      m_stats.TimeAfterUpdate = m_timer.GetTimestamp();
    }

    ManageExitRequests(false);
    CacheState();

    // Let the caller know update has been called
    return ProcessOnDemandRendering();
  }

  AppDrawResult DemoAppManager::TryDraw()
  {
    const FrameInfo frameInfo(m_record.FrameIndex, m_currentDemoTimeDraw);

    const auto result = [this, &frameInfo]()
    {
      const ScopedTraceZone traceZone(m_trace.get(), m_traceZonePrepareDraw);
      return m_record.DemoApp->_TryPrepareDraw(frameInfo);
    }();
    if (result != AppDrawResult::Completed)
    {
      return result;
    }

    if (m_framePacingMarkerServiceControl)
    {
      // The frame's CPU work starts with the app update
      m_framePacingMarkerServiceControl->BeginFrame(frameInfo, m_stats.TimeBeforeUpdate);
    }
    if (m_trace)
    {
      ITraceService& rLog = *m_trace;
      m_framePacingLogFrameIndex = rLog.GetFrameIndex();
      m_framePacingLogHasFrame = true;
      rLog.SetValue(m_framePacingLogColumns.UpdateEnd, m_stats.TimeAfterUpdate);
      rLog.SetUInt64(m_framePacingLogColumns.FrameSlot, frameInfo.FrameIndex);
      rLog.SetInt64(m_framePacingLogColumns.FrameworkTime, frameInfo.Time.CurrentTickCount.Ticks());
      rLog.SetValue(m_framePacingLogColumns.FrameworkStep, frameInfo.Time.ElapsedTime);
      if (m_demoAppConfig.WindowMetrics.ExtentPx != m_framePacingLogExtentPx)
      {
        // The window the frames are drawn to, written when it changes
        m_framePacingLogExtentPx = m_demoAppConfig.WindowMetrics.ExtentPx;
        const DemoWindowMetrics& metrics = m_demoAppConfig.WindowMetrics;
        rLog.AddEvent("window", fmt::format("widthPx={};heightPx={};exactDpiX={};exactDpiY={};densityDpi={}", metrics.ExtentPx.Width.Value,
                                            metrics.ExtentPx.Height.Value, metrics.ExactDpi.X, metrics.ExactDpi.Y, metrics.DensityDpi));
      }
      if (m_framePacingLogRefreshIntervalNs < 0)
      {
        // The window does not exist when the manager is created, so it is found at the first frame
        const auto windowHostInfo = m_demoAppConfig.DemoServiceProvider.TryGet<IWindowHostInfo>();
        if (windowHostInfo)
        {
          const auto windows = windowHostInfo->GetWindows();
          if (!windows.empty())
          {
            m_framePacingLogWindow = windows.front();
            const auto firstWindow = m_framePacingLogWindow.lock();
            if (firstWindow)
            {
              // What the window system has that tells when a frame is shown and what of it is used, so the log says what the platform offers
              const NativeWindowTimingSupport support = firstWindow->GetTimingSupport();
              rLog.SetFact("window.system", support.WindowSystem);
              rLog.SetFact("window.vsyncSource", support.VSyncSource);
              for (const auto& entry : support.Available)
              {
                rLog.SetFact(fmt::format("window.has.{}", entry), "1");
              }
              for (const auto& entry : support.NotAvailable)
              {
                rLog.SetFact(fmt::format("window.has.{}", entry), "0");
              }
              for (const auto& entry : support.Versions)
              {
                rLog.SetFact(fmt::format("window.version.{}", entry.first), fmt::format("{}", entry.second));
              }
              for (const auto& entry : support.Used)
              {
                rLog.SetFact(fmt::format("window.uses.{}", entry), "1");
              }
              // The vsync sources the window system has code for and what each can do here
              rLog.SetFact("window.vsyncSourceRequested", support.RequestedVSyncSource);
              std::string strSources;
              for (const auto& entry : support.VSyncSources)
              {
                const char* const pszState = entry.State == NativeWindowVSyncSourceState::Used
                                               ? "used"
                                               : (entry.State == NativeWindowVSyncSourceState::Available ? "available" : "notAvailable");
                rLog.SetFact(fmt::format("window.vsyncSource.{}", entry.Name), pszState);
                fmt::format_to(std::back_inserter(strSources), "{}{}: {}", strSources.empty() ? "" : ", ", entry.Name, pszState);
              }
              FSLLOG3_INFO("FramePacing: vsync sources of the window system [{}], asked for '{}'", strSources,
                           support.RequestedVSyncSource.empty() ? "auto" : support.RequestedVSyncSource);
              FSLLOG3_INFO("FramePacing: window system '{}', vsync source '{}', has [{}], does not have [{}], uses [{}]", support.WindowSystem,
                           support.VSyncSource.empty() ? "none" : support.VSyncSource, fmt::join(support.Available, ", "),
                           fmt::join(support.NotAvailable, ", "), fmt::join(support.Used, ", "));
            }
          }
        }
      }
      {
        // The refresh interval of the display as the window system reports it (zero: not known), written when it changes.
        // Reading it is cheap as the window caches it.
        const auto window = m_framePacingLogWindow.lock();
        const NanosecondTimeSpan refreshInterval = window ? window->TryGetDisplayInfo().RefreshInterval : NanosecondTimeSpan();
        if (window)
        {
          // When the display refreshes according to the window system, so it can be compared with when the frames were shown
          const NativeWindowVSyncInfo vsyncInfo = window->TryGetVSyncInfo();
          if (vsyncInfo.IsValid())
          {
            // The times of the log are in ticks. The period is too coarse in ticks, so it is logged in nanoseconds as well.
            rLog.SetValue(m_framePacingLogColumns.DisplayVSync, NanosecondTickCountUtil::ToTickCount(vsyncInfo.VSyncTime));
            rLog.SetValue(m_framePacingLogColumns.DisplayRefreshPeriod, NanosecondTimeSpanUtil::ToTimeSpan(vsyncInfo.RefreshPeriod));
            rLog.SetInt64(m_framePacingLogColumns.DisplayRefreshPeriodNs, vsyncInfo.RefreshPeriod.TotalNanoseconds());
            rLog.SetUInt64(m_framePacingLogColumns.DisplayVSyncFlags, NativeWindowVSyncTimeFlagsUtil::ToLogCode(vsyncInfo.TimeFlags));
          }
          // What is known about variable refresh: the measurement per frame, the answers as a event when one of them changes
          const NativeWindowVariableRefreshInfo variableRefresh = window->TryGetVariableRefreshInfo();
          if (variableRefresh.ObservedIntervalCount != 0u)
          {
            rLog.SetUInt64(m_framePacingLogColumns.DisplayVBlankInterval, variableRefresh.ObservedIntervalMilliPeriods);
            rLog.SetUInt64(m_framePacingLogColumns.DisplayVBlankOffPeriod, variableRefresh.ObservedOffPeriodPerMille);
          }
          const int32_t packedAnswers = (static_cast<int32_t>(variableRefresh.Supported) << 12) |
                                        (static_cast<int32_t>(variableRefresh.Enabled) << 8) | (static_cast<int32_t>(variableRefresh.Active) << 4) |
                                        static_cast<int32_t>(variableRefresh.Observed);
          if (packedAnswers != m_framePacingLogVariableRefresh)
          {
            m_framePacingLogVariableRefresh = packedAnswers;
            const auto toText = [](const NativeWindowVariableRefreshAnswer answer)
            {
              switch (answer)
              {
              case NativeWindowVariableRefreshAnswer::No:
                return "no";
              case NativeWindowVariableRefreshAnswer::Yes:
                return "yes";
              case NativeWindowVariableRefreshAnswer::Unknown:
              default:
                return "unknown";
              }
            };
            rLog.AddEvent("variableRefresh",
                          fmt::format("supported={};enabled={};active={};observed={};source={};observedSource={}", toText(variableRefresh.Supported),
                                      toText(variableRefresh.Enabled), toText(variableRefresh.Active), toText(variableRefresh.Observed),
                                      variableRefresh.Source, variableRefresh.ObservedSource));
          }
        }
        if (refreshInterval.TotalNanoseconds() != m_framePacingLogRefreshIntervalNs)
        {
          m_framePacingLogRefreshIntervalNs = refreshInterval.TotalNanoseconds();
          // In nanoseconds, and rounded to the ticks the times of the log are in
          rLog.AddEvent("display", fmt::format("refreshIntervalTicks={};refreshIntervalNs={}",
                                               NanosecondTimeSpanUtil::ToTimeSpan(refreshInterval).Ticks(), refreshInterval.TotalNanoseconds()));
        }
      }
    }

    const ScopedTraceZone traceZoneDraw(m_trace.get(), m_traceZoneDraw);
    {
      const ScopedTraceZone traceZoneStage(m_trace.get(), m_traceZoneBeginDraw);
      m_record.DemoApp->_BeginDraw(frameInfo);
    }
    try
    {
      {
        const ScopedTraceZone traceZoneStage(m_trace.get(), m_traceZoneAppDraw);
        m_record.DemoApp->_Draw(frameInfo);
      }
      // The frame pacing marker must be the last thing the app frame draws (it is rendered with the basic render system, so it has to be
      // drawn inside the frame)
      if (m_framePacingOverlay && m_state == DemoState::Running)
      {
        const ScopedTraceZone traceZoneStage(m_trace.get(), m_traceZoneMarkerDraw);
        m_framePacingOverlay->Draw(m_demoAppConfig.WindowMetrics);
      }
      const ScopedTraceZone traceZoneStage(m_trace.get(), m_traceZoneEndDraw);
      m_record.DemoApp->_EndDraw(frameInfo);
    }
    catch (std::exception& ex)
    {
      FSLLOG3_ERROR("Exception during draw: {}", ex.what());
      m_record.DemoApp->_EndDraw(frameInfo);
      throw;
    }

    m_stats.TimeAfterDraw = m_timer.GetTimestamp();
    if (m_trace)
    {
      m_trace->SetValueAt(m_framePacingLogFrameIndex, m_framePacingLogColumns.DrawEnd, m_stats.TimeAfterDraw);
    }

    if (m_enableStats && m_state == DemoState::Running && m_demoAppProfilerOverlay)
    {
      const ScopedTraceZone traceZoneStage(m_trace.get(), m_traceZoneProfilerDraw);
      m_demoAppProfilerOverlay->Draw(m_demoAppConfig.WindowMetrics);
    }

    ManageExitRequests(false);

    return result;
  }


  void DemoAppManager::OnDrawSkipped()
  {
    if (m_record.DemoApp)
    {
      const FrameInfo frameInfo(m_record.FrameIndex, m_currentDemoTimeDraw);
      m_record.DemoApp->_OnDrawSkipped(frameInfo);
    }
  }


  AppDrawResult DemoAppManager::TryAppSwapBuffers()
  {
    if (!m_record.DemoApp)
    {
      return AppDrawResult::Completed;
    }
    const FrameInfo frameInfo(m_record.FrameIndex, m_currentDemoTimeDraw);

    const AppDrawResult result = [this, &frameInfo]()
    {
      const ScopedTraceZone traceZone(m_trace.get(), m_traceZoneAppSwap);
      return m_record.DemoApp->_TrySwapBuffers(frameInfo);
    }();

    if (result == AppDrawResult::Completed)
    {    // Increase the frame index
      ++m_record.FrameIndex;
      m_record.FrameIndex %= m_demoAppControl->GetRenderLoopFrameCounter();
      assert(m_record.FrameIndex < m_demoAppSetup.CustomAppConfig.MaxFramesInFlight);
    }
    return result;
  }


  void DemoAppManager::OnActivate()
  {
    assert(m_demoAppControl);
    m_demoAppControl->RequestUpdateTimerReset();
  }


  void DemoAppManager::OnDeactivate()
  {
  }


  void DemoAppManager::OnSwapBuffers(const TickCount callTime, const TickCount returnTime) noexcept
  {
    if (m_trace && m_framePacingLogHasFrame)
    {
      m_trace->SetValueAt(m_framePacingLogFrameIndex, m_framePacingLogColumns.SwapCall, callTime);
      m_trace->SetValueAt(m_framePacingLogFrameIndex, m_framePacingLogColumns.SwapReturn, returnTime);
    }
  }


  void DemoAppManager::OnFrameSwapCompleted()
  {
    if (m_state == DemoState::Running)
    {
      UpdateAppTimers();

      const auto deltaTimeUpdate = m_stats.TimeAfterUpdate - m_stats.TimeBeforeUpdate;
      const auto deltaTimeDraw = m_stats.TimeAfterDraw - m_stats.TimeAfterUpdate;

      const auto timeNow = m_timer.GetTimestamp();
      const auto deltaFrameSwapCompletedTime = timeNow - m_stats.LastFrameSwapCompletedTime;
      m_stats.LastFrameSwapCompletedTime = timeNow;
      if (m_trace && m_framePacingLogHasFrame)
      {
        m_trace->SetValueAt(m_framePacingLogFrameIndex, m_framePacingLogColumns.SwapCompleted, timeNow);
      }

      m_profilerServiceControl->AddFrameTimes(TimeSpanUtil::ToClampedMicrosecondsUInt64(deltaTimeUpdate),
                                              TimeSpanUtil::ToClampedMicrosecondsUInt64(deltaTimeDraw),
                                              TimeSpanUtil::ToClampedMicrosecondsUInt64(deltaFrameSwapCompletedTime));

      const auto averageTime = m_profilerService->GetAverageFrameTime();
      const auto averageTotalTime = TimeSpanUtil::FromMicroseconds(averageTime.TotalTime);
      const float frameFps = deltaFrameSwapCompletedTime.Ticks() > 0
                               ? (static_cast<float>(TimeSpan::TicksPerSecond) / static_cast<float>(deltaFrameSwapCompletedTime.Ticks()))
                               : 0.0f;
      const float averageFps =
        averageTotalTime.Ticks() > 0 ? (static_cast<float>(TimeSpan::TicksPerSecond) / static_cast<float>(averageTotalTime.Ticks())) : 0.0f;

      if (m_logStatsFlags.IsFlagged(DemoAppStatsFlags::CPU) && m_cpuStatsService)
      {
        float cpuUsage = 0.0f;
        m_cpuStatsService->TryGetApplicationCpuUsage(cpuUsage);

        if (m_logStatsFlags.IsFlagged(DemoAppStatsFlags::Frame))
        {
          // Flags: Frame | CPU
          FSLLOG3_INFO_IF(m_logStatsMode == LogStatsMode::Latest, "All: {} FPS: {} Updates: {} Draw: {} CPU: {}",
                          TimeSpanUtil::ToClampedMicrosecondsUInt64(deltaFrameSwapCompletedTime), frameFps,
                          TimeSpanUtil::ToClampedMicrosecondsUInt64(deltaTimeUpdate), TimeSpanUtil::ToClampedMicrosecondsUInt64(deltaTimeDraw),
                          cpuUsage);
          FSLLOG3_INFO_IF(m_logStatsMode == LogStatsMode::Average, "Average All: {} FPS: {} Updates: {} Draw: {} CPU: {}",
                          TimeSpanUtil::ToClampedMicrosecondsUInt64(averageTotalTime), averageFps, averageTime.UpdateTime, averageTime.DrawTime,
                          cpuUsage);
        }
        else
        {
          // Flags: CPU
          FSLLOG3_INFO_IF(m_logStatsMode == LogStatsMode::Latest, "CPU: {}", cpuUsage);
          FSLLOG3_INFO_IF(m_logStatsMode == LogStatsMode::Average, "CPU: {}", cpuUsage);
        }
      }
      else
      {
        // Flags: Frame
        FSLLOG3_INFO_IF(m_logStatsMode == LogStatsMode::Latest, "All: {} FPS: {} Updates: {} Draw: {}",
                        TimeSpanUtil::ToClampedMicrosecondsUInt64(deltaFrameSwapCompletedTime), frameFps,
                        TimeSpanUtil::ToClampedMicrosecondsUInt64(deltaTimeUpdate), TimeSpanUtil::ToClampedMicrosecondsUInt64(deltaTimeDraw));
        FSLLOG3_INFO_IF(m_logStatsMode == LogStatsMode::Average, "Average All: {} FPS: {} Updates: {} Draw: {}", averageTime.TotalTime, averageFps,
                        averageTime.UpdateTime, averageTime.DrawTime);
      }

      if (m_logStatsMode != LogStatsMode::Disabled && m_logStatsFlags.IsFlagged(DemoAppStatsFlags::GPU) && m_systemStatsService)
      {
        // Flags: GPU. It has a line of its own, so the lines above stay what they were. It is only asked for when it is logged, as the
        // first request starts the measuring. The load is in percent and the memory in bytes (zero while not available).
        GpuUsageRecord gpuUsage;
        m_systemStatsService->TryGetApplicationGpuUsage(gpuUsage);
        GpuMemoryUsageRecord gpuMemoryUsage;
        m_systemStatsService->TryGetApplicationGpuMemoryUsage(gpuMemoryUsage);
        FSLLOG3_INFO("GPU: {} GPUMem: {}", gpuUsage.UsagePercentage, gpuMemoryUsage.TotalBytes());
      }
    }
  }


  void DemoAppManager::OnDemandDrawSkipped()
  {
    if (m_state == DemoState::Running)
    {
      UpdateAppTimers();
    }
  }


  void DemoAppManager::ProcessDone()
  {
    if (m_state == DemoState::Running)
    {
      if (m_record.DemoApp)
      {
        m_record.DemoApp->_End();
      }
    }
  }


  void DemoAppManager::RequestExit()
  {
    m_hasExitRequest = true;
  }


  bool DemoAppManager::HasExitRequest() const
  {
    return m_hasExitRequest;
  }

  bool DemoAppManager::HasRestartRequest() const
  {
    return m_demoAppControl && m_demoAppControl->HasAppRestartRequest();
  }


  int DemoAppManager::CloseApp()
  {
    // Ensure that the app has been marked as requesting a exit
    if (!m_demoAppControl->HasExitRequest())
    {
      m_demoAppControl->RequestExit();
    }

    // Free the app
    DoShutdownAppNow();
    return m_demoAppControl->GetExitCode();
  }

  void DemoAppManager::CacheState()
  {
    m_cachedState.CachedTimeStepMode = m_demoAppControl->GetTimeStepMode();
  }


  void DemoAppManager::UpdateAppTimers()
  {
    // Apply any changes that might have occurred to the fixed update per seconds setting
    m_appTiming.SetFixedUpdatesPerSecond(m_demoAppControl->GetFixedUpdatesPerSecond());

    // If we can get a timestamp for when the last 'display buffer swap' occurred we could make this much more precise.
    m_appTiming.TimeNow(m_timer.GetTimestamp(), m_cachedState.CachedTimeStepMode);
  }


  DemoAppManagerProcessResult DemoAppManager::ProcessOnDemandRendering()
  {
    // FSLLOG3_VERBOSE("{}", m_currentDemoTimeUpdate.DeltaTimeInMicroseconds);
    const uint16_t onDemandFrameInterval = m_demoAppControl->GetOnDemandFrameInterval();
    if (onDemandFrameInterval <= 1)
    {
      m_currentDemoTimeDraw = m_appTiming.GetUpdateTime();
      m_onDemandRendering = {};
      return DemoAppManagerProcessResult(DemoAppManagerProcessResult::Command::Draw);
    }
    // FIX: we need to ensure that at least one frame is currently visible, we need more info from the 'owner' as the present could have failed
    if (onDemandFrameInterval != m_onDemandRendering.LastOnDemandFrameInterval)
    {
      const double wait = 60.0 / onDemandFrameInterval;
      const double waitTime = wait > 0 ? 1000000.0 / wait : 1000000.0;

      // Render the first frame after its been enabled
      const auto waitTimeInMicroseconds = NumericCast<uint64_t>(static_cast<int64_t>(std::round(waitTime)));
      m_onDemandRendering = OnDemandRendering{onDemandFrameInterval, waitTimeInMicroseconds, 0};
      m_currentDemoTimeDraw = m_appTiming.GetUpdateTime();
      return DemoAppManagerProcessResult(DemoAppManagerProcessResult::Command::Draw);
    }
    if (m_onDemandRendering.ForceRenderNextFrame)
    {
      // We do this to ensure that any app changes that might be frame delayed gets rendered (this is nice for a UI that tries to show the active
      // OnDemandFrameInterval)
      m_onDemandRendering.ForceRenderNextFrame = false;
      m_currentDemoTimeDraw = m_appTiming.GetUpdateTime();
      return DemoAppManagerProcessResult(DemoAppManagerProcessResult::Command::Draw);
    }

    m_onDemandRendering.TimeInTicks += m_appTiming.GetUpdateTime().ElapsedTime.Ticks();
    if (m_onDemandRendering.TimeInTicks >= m_onDemandRendering.WaitTimeInTicks)
    {
      // const uint64_t overshoot = m_onDemandRendering.TimeInMicroseconds - m_onDemandRendering.WaitTimeInMicroseconds;
      // FSLLOG3_INFO("DRAW target time {}, time passed {}, diff: {}, skipped: {}", m_onDemandRendering.WaitTimeInMicroseconds,
      //             m_onDemandRendering.TimeInMicroseconds, overshoot, m_onDemandRendering.SkipCount);
      m_currentDemoTimeDraw = m_appTiming.GetUpdateTime();
      m_onDemandRendering.TimeInTicks = 0;
      m_onDemandRendering.SkipCount = 0;
      return DemoAppManagerProcessResult(DemoAppManagerProcessResult::Command::Draw);
    }

    constexpr uint64_t DefaultFramesPerSecond = 60;
    const uint64_t waitTimeLeft = m_onDemandRendering.WaitTimeInTicks - m_onDemandRendering.TimeInTicks;
    const uint64_t defaultFrameTime = 1000000 / DefaultFramesPerSecond;
    const uint64_t sleepTime = defaultFrameTime <= waitTimeLeft ? defaultFrameTime : waitTimeLeft;
    ++m_onDemandRendering.SkipCount;
    return DemoAppManagerProcessResult(DemoAppManagerProcessResult::Command::SkipDrawSleep, sleepTime);
  }


  bool DemoAppManager::ManageExitRequests(const bool bCheckExternalOnly)
  {
    assert(m_demoAppControl);

    const bool bExitRightAway = !bCheckExternalOnly && !m_record.DemoApp;

    if (!m_hasExitRequest && m_demoAppControl->HasExitRequest())
    {
      m_hasExitRequest = true;
    }
    return bExitRightAway;
  }


  void DemoAppManager::ManageAppState(const DemoWindowMetrics& windowMetrics, const bool isConsoleBasedApp)
  {
    assert(m_demoAppControl);

    bool applyFirewall = m_useFirewall;

    const bool restartRequest = HasRestartRequest();
    if (restartRequest)
    {
      m_demoAppControl->ClearAppRestartRequestRequest();
      applyFirewall = true;
    }

    if (m_record.DemoApp &&
        (restartRequest || CheckRestartFlags(m_demoAppSetup.CustomAppConfig.RestartFlags, windowMetrics, m_demoAppConfig.WindowMetrics)))
    {
      // Release the app
      DoShutdownAppNow();
    }

    // Check if a exit request exist (this catches the rare case where the exit occurs during a screen resolution change was detected
    // and the app was discarded above and it requested a exit during destruction) thereby allowing a fast exit.
    if (!m_record.DemoApp && m_demoAppControl->HasExitRequest())
    {
      return;
    }

    // Handle delayed app initialization
    if (!m_record.DemoApp)
    {
      m_demoAppConfig.UpdateWindowMetrics(windowMetrics);
      if (m_graphicsService)
      {
        m_graphicsService->SetWindowMetrics(windowMetrics);
      }
      if (!applyFirewall && ((windowMetrics.ExtentPx != PxExtent2D::Create(0, 0)) || isConsoleBasedApp))
      {
        m_record = AppRecord(m_demoAppSetup.Factory->Allocate(m_demoAppConfig));
      }
      else
      {
        m_record = AppRecord(std::make_shared<DemoAppFirewall>(m_demoAppConfig, m_demoAppSetup.Factory, isConsoleBasedApp));
      }

      m_record.DemoApp->_PostConstruct();

      m_eventListener->SetDemoApp(m_record.DemoApp);
      assert(m_record.DemoApp);
    }
  }


  void DemoAppManager::ResetTimer()
  {
    // Apply any changes that might have occurred to the fixed update per seconds setting
    m_appTiming.SetFixedUpdatesPerSecond(m_demoAppControl->GetFixedUpdatesPerSecond());

    const auto currentTime = m_timer.GetTimestamp();
    m_appTiming.ResetTimer(currentTime);
    m_appTiming.AdvanceFixedTimeStep();
    m_onDemandRendering = {};
    m_stats = {};
    m_stats.LastFrameSwapCompletedTime = currentTime;
  }


  void DemoAppManager::DoShutdownAppNow()
  {
    if (m_record.DemoApp)
    {
      try
      {
        m_record.DemoApp->_PreDestruct();
      }
      catch (const std::exception& ex)
      {
        FSLLOG3_ERROR("Exception throw in _PreDestruct: {}", ex.what());
        m_record = {};
        throw;
      }
      m_record = {};

      assert(m_demoAppControl);
      m_demoAppControl->RestoreDefaults();
    }
  }
}
