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
#include <FslBase/Math/ConstrainedValue.hpp>
#include <FslBase/Math/Pixel/PxRectangle.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Span/SpanUtil_Array.hpp>
#include <FslBase/Time/NanosecondTickCountUtil.hpp>
#include <FslBase/Time/NanosecondTimeSpanUtil.hpp>
#include <FslDemoApp/Base/Service/Events/Basic/WindowFocusEvent.hpp>
#include <FslDemoApp/Shared/Host/DemoWindowMetrics.hpp>
#include <FslDemoHost/Base/Service/WindowHost/IWindowHostInfo.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingFrameSchedule.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingMarkerInfo.hpp>
#include <FslDemoService/FramePacingMarker/IFramePacingMarkerService.hpp>
#include <FslDemoService/Graphics/IGraphicsService.hpp>
#include <FslDemoService/Trace/ITraceService.hpp>
#include <FslDemoService/Trace/ScopedTraceZone.hpp>
#include <FslGraphics/Bitmap/ReadOnlyRawBitmap.hpp>
#include <FslGraphics/Colors.hpp>
#include <FslGraphics/Render/Adapter/INativeBatch2D.hpp>
#include <FslNativeWindow/Base/INativeWindow.hpp>
#include <FslNativeWindow/Base/NativeWindowDisplayInfo.hpp>
#include <FslNativeWindow/Base/NativeWindowTimingSupport.hpp>
#include <FslNativeWindow/Base/NativeWindowVSyncInfo.hpp>
#include <FslNativeWindow/Base/NativeWindowVariableRefreshInfo.hpp>
#include <FslNativeWindow/Base/VirtualKey.hpp>
#include <FslSimpleUI/App/Theme/ThemeSelector.hpp>
#include <FslSimpleUI/Base/Control/Background.hpp>
#include <FslSimpleUI/Base/Control/Image.hpp>
#include <FslSimpleUI/Base/Control/ScrollViewer.hpp>
#include <FslSimpleUI/Base/Event/WindowSelectEvent.hpp>
#include <FslSimpleUI/Base/Layout/DockLayout.hpp>
#include <FslSimpleUI/Base/Layout/GridLayout.hpp>
#include <FslSimpleUI/Base/Layout/StackLayout.hpp>
#include <FslSimpleUI/Controls/Charts/AreaChart.hpp>
#include <FslSimpleUI/Controls/Charts/Common/ChartGridLinesFps.hpp>
#include <FslSimpleUI/Controls/Charts/Data/ChartData.hpp>
#include <FslSimpleUI/Theme/Base/IThemeControlFactory.hpp>
#include <FslSimpleUI/Theme/Base/IThemeResources.hpp>
#include <FslSimpleUI/Theme/Base/WindowType.hpp>
#include <Shared/FramePacing/FramePacingShared.hpp>
#include <Shared/FramePacing/OptionParser.hpp>
#include <Shared/FramePacing/SampleAnimationErrorChart.hpp>
#include <Shared/FramePacing/SampleConfig.hpp>
#include <Shared/FramePacing/SampleExplicitSyncUtil.hpp>
#include <Shared/FramePacing/SampleStatsLevelUtil.hpp>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr IO::PathView MenuAtlas("UIAtlas/UIAtlas_160dpi");
      //! The bar sweeps across the screen in this time
      constexpr double SweepSeconds = 2.0;
      constexpr int32_t BarWidthPx = 16;
      constexpr int32_t BoxSizePx = 96;
      //! Two refresh rates are the same if they differ by less than this share
      constexpr double SameRefreshRateTolerance = 0.02;
      //! The raymarched background: the flight through the lattice repeats after this time
      constexpr double TravelSeconds = 40.0;
      //! The raymarched background: the time of one sway of the camera (the waves and the pulses of light run with it)
      constexpr double SwaySeconds = 29.0;
      //! The raymarched background: the time the shape and the colors of the lattice change in
      constexpr double MorphSeconds = 61.0;
      //! WaitUntil only sleeps when the wait is longer than this (a sleeping thread can wake this late), a shorter wait yields
      constexpr TimeSpan CoarseSleepThreshold(20 * TimeSpan::TicksPerMillisecond);
      constexpr TimeSpan CoarseSleepMargin(16 * TimeSpan::TicksPerMillisecond);
    }

    const char* ToString(const FramePacingRunState state) noexcept
    {
      switch (state)
      {
      case FramePacingRunState::Idle:
        return "idle";
      case FramePacingRunState::Starting:
        return "start marker";
      case FramePacingRunState::Measuring:
        return "measuring";
      case FramePacingRunState::Ending:
        return "end marker";
      default:
        return "unknown";
      }
    }

    const char* ToString(const FramePacingMarkerKind kind) noexcept
    {
      switch (kind)
      {
      case FramePacingMarkerKind::Frame:
        return "frame";
      case FramePacingMarkerKind::SequenceStart:
        return "start";
      case FramePacingMarkerKind::SequenceEnd:
        return "end";
      default:
        return "unknown";
      }
    }

    //! The name of a kind of the pacer as --Pacer.Kind takes it
    const char* ToString(const SamplePacerKind kind) noexcept
    {
      switch (kind)
      {
      case SamplePacerKind::TimerWaitForPresent:
        return "timer-present-wait";
      case SamplePacerKind::VBlankPeriodOnly:
        return "vblank-period";
      case SamplePacerKind::VBlankWaitForPresent:
        return "vblank-present-wait";
      case SamplePacerKind::TimerPeriodOnly:
      default:
        return "timer-period";
      }
    }

    const char* ToString(const SamplePacerAim aim) noexcept
    {
      switch (aim)
      {
      case SamplePacerAim::LowLatency:
        return "low-latency";
      case SamplePacerAim::Smoothness:
      default:
        return "smoothness";
      }
    }

    using SamplePacerTierChoiceUtil::IsPresentWaitKind;
    using SamplePacerTierChoiceUtil::IsVBlankKind;

    //! The value of pacerKind in the trace
    int64_t ToLogCode(const SamplePacerKind kind) noexcept
    {
      switch (kind)
      {
      case SamplePacerKind::TimerWaitForPresent:
        return 2;
      case SamplePacerKind::VBlankPeriodOnly:
        return 3;
      case SamplePacerKind::VBlankWaitForPresent:
        return 4;
      case SamplePacerKind::TimerPeriodOnly:
      default:
        return 1;
      }
    }

    //! The text of the radio button of a tier of the pacer library
    std::string ToTierLabel(const SamplePacerTier tier)
    {
      return fmt::format("Tier {}: {}", SamplePacer::GetTierNumber(tier), SamplePacer::GetTierName(tier));
    }

    //! The name of a scene of the background as --Background takes it
    const char* ToLogName(const RaymarchScene scene) noexcept
    {
      switch (scene)
      {
      case RaymarchScene::Hall:
        return "hall";
      case RaymarchScene::Blobs:
        return "blobs";
      case RaymarchScene::Lace:
        return "lace";
      case RaymarchScene::Mandelbrot:
        return "mandelbrot";
      case RaymarchScene::Flight:
      default:
        return "flight";
      }
    }

    //! Shown for a value the marker reports as unknown
    constexpr const char* UnknownValue = "unknown";
    //! Shown for a value only the frame pacer has while it is off
    constexpr const char* PacerOffValue = "pacer off";
    //! Shown for the values that need the app to measure when its frames are presented
    constexpr const char* NotMeasuredValue = "not measured";
    constexpr const char* NotSupportedValue = "not supported";
    constexpr const char* SwitchedOffValue = "switched off";
    //! The width of the name column of the stats panel, so the values of its sections line up
    constexpr float StatsNameColumnWidthDp = 216.0f;
    constexpr float StatsValueColumnWidthDp = 360.0f;

    //! The chart of the work per frame: the three channels and their colors
    constexpr uint32_t WorkChartChannelCount = 3;
    constexpr uint32_t WorkChartCpuChannel = 0;
    constexpr uint32_t WorkChartGpuChannel = 1;
    constexpr uint32_t WorkChartFrameChannel = 2;
    constexpr UI::UIColor WorkChartCpuColor(PackedColor32(0xFF3488A7));      // light blue
    constexpr UI::UIColor WorkChartGpuColor(PackedColor32(0xFFE0902A));      // orange
    constexpr UI::UIColor WorkChartFrameColor(PackedColor32(0xFFFFFFFF));    // white: the total, it stands out from the two it is made of
    constexpr float WorkChartHeightDp = 100.0f;
    //! The legends of the charts are as wide as each other and do not follow their text, so the charts are as wide as each other
    //! and do not move when a number in a legend gets another digit
    constexpr float ChartLegendWidthDp = 260.0f;
    //! How often the numbers of a legend are written
    constexpr TimeSpan ChartLegendInterval = TimeSpan::FromMilliseconds(250);

    //! The colors of a value of the frame pacing overlay that asks to be looked at
    constexpr UI::UIColor StatsWarningColor(PackedColor32(0xFFFFC233));    // yellow
    constexpr UI::UIColor StatsErrorColor(PackedColor32(0xFFFF5A4D));      // red
    //! The colors of the tiers of the pacer library, the best tier first. They are the ones a well known game gives the quality of
    //! its items (legendary, epic, rare, uncommon), the last three a little lighter so they can be read on the side bar.
    constexpr std::array<UI::UIColor, 4> TierColors = {UI::UIColor(PackedColor32(0xFFFF8000)),     // orange
                                                       UI::UIColor(PackedColor32(0xFFC27CFF)),     // purple
                                                       UI::UIColor(PackedColor32(0xFF4FA3FF)),     // blue
                                                       UI::UIColor(PackedColor32(0xFF5FD35F))};    // green

    constexpr UI::UIColor ToColor(const SampleStatsLevel level, const UI::UIColor normalColor) noexcept
    {
      switch (level)
      {
      case SampleStatsLevel::Warning:
        return StatsWarningColor;
      case SampleStatsLevel::Error:
        return StatsErrorColor;
      case SampleStatsLevel::Normal:
        break;
      }
      return normalColor;
    }

    //! The chart of the animation error, in the colors of the report of the mb-framepacing tools
    constexpr UI::UIColor AnimationErrorChartBarColor(PackedColor32(0xFFE5534B));        // red
    constexpr UI::UIColor AnimationErrorChartRefreshColor(PackedColor32(0xFFD29922));    // amber
    constexpr float AnimationErrorChartHeightDp = 120.0f;

    //! A time as the microseconds a chart entry holds
    constexpr uint32_t ToChartMicroseconds(const TimeSpan time) noexcept
    {
      const int64_t microseconds = time.Ticks() / TimeSpan::TicksPerMicrosecond;
      return static_cast<uint32_t>(std::clamp(microseconds, int64_t{0}, int64_t{std::numeric_limits<uint32_t>::max()}));
    }

    //! Where the time is in a cycle of the given length, in [0,1)
    float ToPhase(const double seconds, const double cycleSeconds) noexcept
    {
      const double phase = std::fmod(seconds / cycleSeconds, 1.0);
      return static_cast<float>(phase >= 0.0 ? phase : (phase + 1.0));
    }

    //! A slider that starts at the value of a command line option
    ConstrainedValue<int32_t> WithValue(const ConstrainedValue<int32_t> range, const int32_t value) noexcept
    {
      return ConstrainedValue<int32_t>(value, range.Min(), range.Max());
    }
  }


  FramePacingShared::FramePacingShared(const DemoAppConfig& config, std::string runName, const SamplePresentMethod presentMethod)
    : m_uiEventListener(this)
    , m_uiExtension(std::make_shared<UIDemoAppExtension>(config, m_uiEventListener.GetListener(), LocalConfig::MenuAtlas))
    , m_framePacing(config.DemoServiceProvider.TryGet<IFramePacingMarkerService>())
    , m_runName(std::move(runName))
    , m_windowSizePx(config.WindowMetrics.GetSizePx())
    , m_presentMethod(presentMethod)
    , m_workChartData(std::make_shared<UI::ChartData>(m_uiExtension->GetDataBinding(), config.WindowMetrics.ExtentPx.Width.Value,
                                                      WorkChartChannelCount, UI::ChartData::Constraints(0, {}), UI::ChartChannelPolicy::Overlaid))
  {
    m_workChartData->SetChannelMetaData(WorkChartCpuChannel, WorkChartCpuColor);
    m_workChartData->SetChannelMetaData(WorkChartGpuChannel, WorkChartGpuColor);
    m_workChartData->SetChannelMetaData(WorkChartFrameChannel, WorkChartFrameColor);

    const auto options = config.GetOptions<OptionParser>();
    // Read before the log is set up, which writes it as a fact
    m_pacerKind = options->GetPacerKind();
    m_waitingPresentsOption = static_cast<uint32_t>(std::max(options->GetPacerWaitingPresents(), 0));
    m_readyPlacePercent = static_cast<uint32_t>(std::max(options->GetPacerReadyPlacePercent(), 0));
    m_kindChangeFrames = static_cast<uint32_t>(std::max(options->GetPacerKindChangeFrames(), 0));
    m_kindChangeIndex = SamplePacerTierChoiceUtil::FindKindChangeIndex(options->GetPacerKind());
    m_systemHoldsLoop = options->IsPacerSystemHoldsLoop();
    m_pacerGpuWait = options->IsPacerGpuWait();
    m_pacerSystemWaits = options->IsPacerSystemWaits();
    SetRequestedKind(m_pacerKind);
    m_cpuSpikeMs = options->GetCpuSpikeMs();
    m_cpuSpikeIntervalFrames = options->GetCpuSpikeIntervalFrames();
    // Only a app whose present is a swap has a use for it (SamplePresentMethod::SwapInterval), a Vulkan app submits its frame
    m_flushWanted = options->IsGLFlushEnabled() && presentMethod == SamplePresentMethod::SwapInterval;
    m_refreshRateOverrideHz = options->GetPacerRefreshRateHz();
    m_trace = config.DemoServiceProvider.TryGet<ITraceService>();
    if (m_trace && m_trace->IsEnabled())
    {
      // What the sample does on the thread, inside the update and the draw of the app
      ITraceService& rTrace = *m_trace;
      m_traceZones.KeyboardMenu = rTrace.RegisterZone("Keyboard menu");
      m_traceZones.PacerUpdate = rTrace.RegisterZone("Pacer update");
      m_traceZones.TierUpdate = rTrace.RegisterZone("Tier update");
      m_traceZones.StatsUI = rTrace.RegisterZone("Stats UI");
      m_traceZones.WorkChart = rTrace.RegisterZone("Work chart");
      m_traceZones.AnimationError = rTrace.RegisterZone("Animation error");
      m_traceZones.RunUI = rTrace.RegisterZone("Run UI");
      m_traceZones.StartFrame = rTrace.RegisterZone("Start frame");
      m_traceZones.WaitForFrameStart = rTrace.RegisterZone("Wait for frame start");
      m_traceZones.CpuLoad = rTrace.RegisterZone("CPU load");
      m_traceZones.HoldBeforePresent = rTrace.RegisterZone("Hold before present");
      m_traceZones.SampleDraw = rTrace.RegisterZone("Sample draw");
      m_traceZones.DrawAnimation = rTrace.RegisterZone("Draw animation");
      m_traceZones.DrawBoxAnimation = rTrace.RegisterZone("Draw box animation");
      m_traceZones.EndFrame = rTrace.RegisterZone("End frame");
      m_traceZones.WaitForPresent = rTrace.RegisterZone("Wait for present");
      m_traceZones.BackgroundDraw = rTrace.RegisterZone("Background draw");
      m_traceZones.GpuTimer = rTrace.RegisterZone("GPU timer");
      m_traceZones.Measurements = rTrace.RegisterZone("Measurements");
      m_traceZones.Flush = rTrace.RegisterZone("Flush");

      // What the sample knows about a frame, and what it shows of it on the timeline of the trace
      RegisterLogColumns();
      // The frame as the pacer of the sample sees it: the wait for its start, its work and the hold before its present
      const TraceTrack frameTrack = rTrace.RegisterTrack("Sample frame", TraceTrackKind::Sequential);
      rTrace.DeclareSpan("wait for frame start", frameTrack, m_logColumns.FrameWaitStart, m_logColumns.FrameStart, TraceLink::NoLink);
      rTrace.DeclareSpan("work", frameTrack, m_logColumns.FrameStart, m_logColumns.EndFrame, TraceLink::NoLink);
      rTrace.DeclareSpan("hold before present", frameTrack, m_logColumns.PresentWaitBegin, m_logColumns.PresentWaitEnd, TraceLink::NoLink);
      // The GPU can work on a frame while the next one is drawn, so its work is drawn in lanes
      const TraceTrack gpuTrack = rTrace.RegisterTrack("GPU", TraceTrackKind::Lanes);
      rTrace.DeclareSpan("GPU work", gpuTrack, m_logColumns.GpuWorkBegin, m_logColumns.GpuWorkEnd, TraceLink::FrameChain);
      const TraceTrack planTrack = rTrace.RegisterTrack("Pacer plan", TraceTrackKind::Sequential);
      rTrace.DeclareMark("next frame start", planTrack, m_logColumns.NextFrameStart, TraceLink::NoLink);
      rTrace.DeclareCounter("CPU work", m_logColumns.WorkCpu);
      rTrace.DeclareCounter("GPU time", m_logColumns.GpuTime);
      rTrace.DeclareCounter("Animation error", m_logColumns.AnimationError);
      rTrace.DeclareCounter("Swap interval", m_logColumns.SwapInterval);
    }
    else
    {
      m_trace.reset();
    }
    {
      // The refresh rate of the display is read from the window every frame, as the window can be moved to another display
      const auto windowHostInfo = config.DemoServiceProvider.TryGet<IWindowHostInfo>();
      if (windowHostInfo)
      {
        const auto windows = windowHostInfo->GetWindows();
        if (!windows.empty())
        {
          m_window = windows.front();
        }
      }
      const auto window = m_window.lock();
      if (window)
      {
        // Read once: it does not change while the window lives, and the call allocates
        const NativeWindowTimingSupport timingSupport = window->GetTimingSupport();
        m_explicitSync = SampleExplicitSyncUtil::ToExplicitSync(timingSupport);
        m_vsyncSourceName = timingSupport.VSyncSource;
      }
    }
    UpdateDetectedRefreshPeriod();

    // The service is only available on platforms that support the marker library
    if (m_framePacing)
    {
      // This sample always shows the marker, other apps enable it with --FramePacing
      m_framePacing->SetEnabled(true);
    }

    const auto graphicsService = config.DemoServiceProvider.Get<IGraphicsService>();
    m_nativeBatch = graphicsService->GetNativeBatch2D();
    {
      constexpr std::array<uint8_t, 4> WhitePixel = {0xFF, 0xFF, 0xFF, 0xFF};
      const auto rawBitmap =
        ReadOnlyRawBitmap::Create(SpanUtil::AsReadOnlySpan(WhitePixel), PxSize2D::Create(1, 1), PixelFormat::R8G8B8A8_UNORM, BitmapOrigin::UpperLeft);
      m_fillTexture.Reset(graphicsService->GetNativeGraphics(), rawBitmap, Texture2DFilterHint::Nearest);
    }

    // Create a small status panel on the right side (the marker is drawn on the left side)
    const auto uiFactory = UI::Theme::ThemeSelector::CreateControlFactory(*m_uiExtension);
    m_ui.LabelStatus = uiFactory->CreateLabel(m_framePacing ? "Frame pacing marker: enabled" : "Frame pacing is not supported on this platform");
    m_ui.DefaultFontColor = m_ui.LabelStatus->GetFontColor();
    m_ui.LabelRun = uiFactory->CreateLabel("");
    m_ui.ButtonRun = uiFactory->CreateTextButton(UI::Theme::ButtonType::Contained, "Start run");
    m_ui.ButtonRun->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.ButtonRun->SetEnabled(m_framePacing != nullptr);
    const auto lblDuration = uiFactory->CreateLabel("Timed run duration (seconds)");
    m_ui.SliderDuration =
      uiFactory->CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, WithValue(SampleConfig::TimedRunSeconds, options->GetTimedRunSeconds()));
    m_ui.SliderDuration->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.ButtonTimedRun = uiFactory->CreateTextButton(UI::Theme::ButtonType::Contained, "Start timed run");
    m_ui.ButtonTimedRun->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.ButtonTimedRun->SetEnabled(m_framePacing != nullptr);
    const auto lblHint = uiFactory->CreateLabel("Space: start/end a run");
    const auto lblHintTimed = uiFactory->CreateLabel("T: start a timed run");
    const auto lblHintPacer = uiFactory->CreateLabel("P: frame pacer on/off");
    const auto lblHintMenu = uiFactory->CreateLabel("Arrows + Enter: menu");
    const auto lblHintReset = uiFactory->CreateLabel("0: reset the settings");

    // The two overlays
    m_ui.SwitchMarkerStats = uiFactory->CreateSwitch("Show the last marker", !options->IsMarkerStatsHidden());
    m_ui.SwitchPacerStats = uiFactory->CreateSwitch("Show the frame pacing", !options->IsPacingStatsHidden());
    // The chart of the work per frame and the test pattern
    m_ui.SwitchWorkChart = uiFactory->CreateSwitch("Show the work chart", !options->IsWorkChartHidden());
    m_ui.SwitchAnimationErrorChart = uiFactory->CreateSwitch("Show the animation error chart", !options->IsAnimationErrorChartHidden());
    m_ui.SwitchTestPattern = uiFactory->CreateSwitch("Show the test pattern", !options->IsTestPatternHidden());
    // The box animation of the mb-framepacing-explained videos, to compare the motion of the sample with them by eye
    m_ui.SwitchBoxAnimation = uiFactory->CreateSwitch("Show the box animation", options->GetBoxAnimation() != SampleBoxAnimationSpeed::Off);
    m_ui.SwitchBoxAnimationFast = uiFactory->CreateSwitch("Fast box animation", options->GetBoxAnimation() == SampleBoxAnimationSpeed::Fast);
    // The sync marker of the service (--FramePacing.SyncMarker draws it from the start)
    m_ui.SwitchSyncMarker = uiFactory->CreateSwitch("Draw the sync marker", m_framePacing && m_framePacing->IsSyncMarkerEnabled());
    m_ui.SwitchSyncMarker->SetEnabled(m_framePacing != nullptr);
    // The optional measurements of the app, so what they add can be seen by switching them off. They stay disabled until the app says it
    // can make them (SetMeasurementSupport).
    m_ui.SwitchPresentTiming = uiFactory->CreateSwitch("Measure the presents", true);
    m_ui.SwitchPresentTiming->SetEnabled(false);
    m_ui.SwitchGpuTimeline = uiFactory->CreateSwitch("Place the GPU work in time", true);
    m_ui.SwitchGpuTimeline->SetEnabled(false);

    // The frame pacer of the sample (the library is not available on every platform)
    const bool pacerSupported = SamplePacer::IsSupported();
    m_ui.SwitchPacer = uiFactory->CreateSwitch("Frame pacer (experimental)", pacerSupported && options->IsPacerEnabled());
    m_ui.SwitchPacer->SetEnabled(pacerSupported);
    m_ui.LabelRefreshRate = uiFactory->CreateLabel("");
    m_ui.SliderRefreshRate = uiFactory->CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, SampleConfig::RefreshRateHz);
    m_ui.SliderRefreshRate->SetAlignmentX(UI::ItemAlignment::Stretch);
    const auto lblTargetFps = uiFactory->CreateLabel("Target fps (0 = display rate)");
    m_ui.SliderTargetFps =
      uiFactory->CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, WithValue(SampleConfig::TargetFps, options->GetPacerTargetFps()));
    m_ui.SliderTargetFps->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.SwitchAdaptive = uiFactory->CreateSwitch("Adaptive swap interval", options->IsPacerAdaptive());
    // The aim of the pacer: it stays disabled until the pacer paces the frames
    m_ui.SwitchLowLatency = uiFactory->CreateSwitch("Low latency (off: smoothness)", options->GetPacerAim() == SamplePacerAim::LowLatency);
    m_ui.SwitchLowLatency->SetEnabled(false);
    // Only for a app whose present takes a time
    m_ui.SwitchTimedPresent = uiFactory->CreateSwitch("Timed present", options->IsPacerTimedPresent());
    m_ui.SwitchTimedPresent->SetEnabled(false);
    // Only for an app that measures its presents, so it stays disabled until the pacer is on and the presents are measured
    m_ui.SwitchPacerFeedback = uiFactory->CreateSwitch("Present feedback to the pacer", options->IsPacerPresentFeedback());
    m_ui.SwitchPacerFeedback->SetEnabled(false);
    // What the frames can be paced with, one group of radio buttons: the tiers of the pacer library the samples reach
    // (Doc/FramePacingPlatformSupport.md) with the best first. They start disabled: ShowTiers enables the ones that can be used and
    // checks the one the run is paced by, once the app has said what it can do.
    const auto pacerGroup = uiFactory->CreateRadioGroup("pacer");
    for (std::size_t i = 0; i < m_ui.RadioTiers.size(); ++i)
    {
      const SamplePacerTier tier = SamplePacerTierChoiceUtil::Tiers[i];
      m_ui.RadioTiers[i] = uiFactory->CreateRadioButton(pacerGroup, ToTierLabel(tier), SamplePacerTierChoiceUtil::ToKind(tier) == m_pacerKind);
      m_ui.RadioTiers[i]->SetFontColorChecked(TierColors[i]);
      m_ui.RadioTiers[i]->SetFontColorUnchecked(TierColors[i]);
      m_ui.RadioTiers[i]->SetEnabled(false);
    }
    {    // The line of the pacer library for every tier, in the order of the radio buttons
      std::vector<std::string> descriptions;
      descriptions.reserve(SamplePacerTierChoiceUtil::Tiers.size());
      for (const SamplePacerTier tier : SamplePacerTierChoiceUtil::Tiers)
      {
        descriptions.emplace_back(SamplePacer::GetTierShortDescription(tier));
      }
      m_ui.LabelTierDescription = uiFactory->CreateSelectorLabel(std::move(descriptions), 0u);
    }
    m_drainRefreshes = options->GetPacerDrainRefreshes();
    const auto lblCpuLoad = uiFactory->CreateLabel("CPU load (ms per frame)");
    m_ui.SliderCpuLoad =
      uiFactory->CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, WithValue(SampleConfig::CpuLoadMs, options->GetCpuLoadMs()));
    m_ui.SliderCpuLoad->SetAlignmentX(UI::ItemAlignment::Stretch);
    const auto lblGpuLoad = uiFactory->CreateLabel("GPU load (steps, 0 = off)");
    m_ui.SliderGpuLoad =
      uiFactory->CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, WithValue(SampleConfig::GpuLoadSteps, options->GetGpuLoadSteps()));
    m_ui.SliderGpuLoad->SetAlignmentX(UI::ItemAlignment::Stretch);
    // The scene the raymarched background shows
    const RaymarchScene background = options->GetBackground();
    const auto backgroundGroup = uiFactory->CreateRadioGroup("background");
    m_ui.RadioBackgroundMandelbrot = uiFactory->CreateRadioButton(backgroundGroup, "Mandelbrot zoom", background == RaymarchScene::Mandelbrot);
    m_ui.RadioBackgroundBlobs = uiFactory->CreateRadioButton(backgroundGroup, "Blobs", background == RaymarchScene::Blobs);
    m_ui.RadioBackgroundLace = uiFactory->CreateRadioButton(backgroundGroup, "Lace", background == RaymarchScene::Lace);
    m_ui.RadioBackgroundFlight = uiFactory->CreateRadioButton(backgroundGroup, "Fractal flight", background == RaymarchScene::Flight);
    m_ui.RadioBackgroundHall = uiFactory->CreateRadioButton(backgroundGroup, "Scrolling hall", background == RaymarchScene::Hall);

    // The controls of the side bar are rows of the keyboard menu, in the order they are stacked in. The row of a slider has its
    // caption too.
    const auto stackLayout = std::make_shared<UI::StackLayout>(uiFactory->GetContext());
    stackLayout->SetOrientation(UI::LayoutOrientation::Vertical);
    stackLayout->SetAlignmentY(UI::ItemAlignment::Center);
    stackLayout->AddChild(m_ui.LabelStatus);
    stackLayout->AddChild(m_ui.LabelRun);
    stackLayout->AddChild(m_keyboardMenu.AddButton(*uiFactory, m_ui.ButtonRun, [this]() { ToggleRun(); }));
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(m_keyboardMenu.AddSlider(*uiFactory, lblDuration, m_ui.SliderDuration));
    stackLayout->AddChild(m_keyboardMenu.AddButton(*uiFactory, m_ui.ButtonTimedRun, [this]() { StartTimedRun(); }));
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchPacer));
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    if (SamplePacer::IsSupported())
    {
      // Without the pacer library there is no pacer to choose
      for (const auto& radioButton : m_ui.RadioTiers)
      {
        stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, radioButton));
      }
      stackLayout->AddChild(m_ui.LabelTierDescription);
      stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    }
    stackLayout->AddChild(m_keyboardMenu.AddSlider(*uiFactory, m_ui.LabelRefreshRate, m_ui.SliderRefreshRate));
    stackLayout->AddChild(m_keyboardMenu.AddSlider(*uiFactory, lblTargetFps, m_ui.SliderTargetFps));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchAdaptive));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchLowLatency));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchTimedPresent));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchPacerFeedback));
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(m_keyboardMenu.AddSlider(*uiFactory, lblCpuLoad, m_ui.SliderCpuLoad));
    stackLayout->AddChild(m_keyboardMenu.AddSlider(*uiFactory, lblGpuLoad, m_ui.SliderGpuLoad));
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.RadioBackgroundMandelbrot));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.RadioBackgroundBlobs));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.RadioBackgroundLace));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.RadioBackgroundFlight));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.RadioBackgroundHall));
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchMarkerStats));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchPacerStats));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchWorkChart));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchAnimationErrorChart));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchTestPattern));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchBoxAnimation));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchBoxAnimationFast));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchSyncMarker));
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchPresentTiming));
    stackLayout->AddChild(m_keyboardMenu.AddToggle(*uiFactory, m_ui.SwitchGpuTimeline));
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(lblHint);
    stackLayout->AddChild(lblHintTimed);
    stackLayout->AddChild(lblHintPacer);
    stackLayout->AddChild(lblHintMenu);
    stackLayout->AddChild(lblHintReset);

    // The window is docked: the bar with the controls at the right, the charts at the bottom of what is left of it, and the overlays
    // with every value of the last marker and the frame pacing stats in the rest. Each of them is shown while its switch is on.
    // A dock layout keeps all of them inside the window, so the overlays, which can ask for more height than a low window has, take
    // no room from the charts and do not make the right bar higher than the window (its controls could then not all be scrolled to).
    const auto mainLayout = std::make_shared<UI::DockLayout>(uiFactory->GetContext());
    mainLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    mainLayout->SetAlignmentY(UI::ItemAlignment::Stretch);
    mainLayout->SetLastChildFill(true);
    mainLayout->SetLimitToAvailableSpace(true);
    // The controls can be scrolled, as a low window does not have room for all of them
    stackLayout->SetMargin(DpThicknessF::Create(0, 0, 8, 0));
    const auto scrollViewer = uiFactory->CreateScrollViewer(stackLayout, UI::ScrollModeFlags::TranslateY, false);
    m_keyboardMenu.SetScrollViewer(scrollViewer);
    const auto rightBar = uiFactory->CreateRightBar(scrollViewer);
    mainLayout->AddChild(rightBar, UI::DockType::Right);
    m_ui.RightBar = rightBar;
    m_ui.WorkChartBar = CreateWorkChartBar(*uiFactory);
    m_ui.AnimationErrorChartBar = CreateAnimationErrorChartBar(*uiFactory);
    {
      // The two charts, each can be hidden
      const auto chartLayout = std::make_shared<UI::StackLayout>(uiFactory->GetContext());
      chartLayout->SetOrientation(UI::LayoutOrientation::Vertical);
      chartLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
      chartLayout->AddChild(m_ui.AnimationErrorChartBar);
      chartLayout->AddChild(m_ui.WorkChartBar);
      mainLayout->AddChild(chartLayout, UI::DockType::Bottom);
    }
    // The last child fills what is left
    mainLayout->AddChild(CreateStatsWindow(*uiFactory), UI::DockType::Left);
    m_uiExtension->SetMainWindow(mainLayout);

    UpdateUI();
    UpdateStatsVisibility();
    UpdateMarkerStats();
    UpdateRefreshRateUI();
    UpdatePacerStats();
  }


  FramePacingShared::~FramePacingShared()
  {
    m_presentFeedback.LogSummary();
  }


  void FramePacingShared::OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent)
  {
    if (theEvent->IsHandled())
    {
      return;
    }
    if (theEvent->GetSource() == m_ui.ButtonRun)
    {
      theEvent->Handled();
      ToggleRun();
    }
    else if (theEvent->GetSource() == m_ui.ButtonTimedRun)
    {
      theEvent->Handled();
      StartTimedRun();
    }
  }


  void FramePacingShared::OnWindowFocusEvent(const WindowFocusEvent& event)
  {
    // The app keeps drawing without the input focus, and a window system can treat such a window differently: the log says when
    if (m_trace)
    {
      m_trace->AddEvent("windowFocus", fmt::format("focused={}", event.IsFocused() ? 1 : 0));
    }
    FSLLOG3_VERBOSE("FramePacing: the window {} the input focus", event.IsFocused() ? "got" : "lost");
  }


  void FramePacingShared::OnClickInput(const std::shared_ptr<UI::WindowInputClickEvent>& /*theEvent*/)
  {
    // The UI is used with the mouse (or a finger), so the cursor of the keys is in the way
    m_keyboardMenu.HideCursor();
  }


  void FramePacingShared::OnKeyEvent(const KeyEvent& event)
  {
    // The arrow keys and return, and the keys that come up (a held key repeats in the menu)
    m_keyboardMenu.OnKeyEvent(event);
    if (event.IsHandled() || !event.IsPressed())
    {
      return;
    }
    switch (event.GetKey())
    {
    case VirtualKey::Space:
      event.Handled();
      ToggleRun();
      break;
    case VirtualKey::T:
      event.Handled();
      StartTimedRun();
      break;
    case VirtualKey::P:
      event.Handled();
      if (m_ui.SwitchPacer->IsEnabled())
      {
        m_ui.SwitchPacer->Toggle();
      }
      break;
    case VirtualKey::Code0:
      // The switches, radio buttons and sliders of the side bar are as the sample started with them
      event.Handled();
      m_keyboardMenu.ResetToDefaults();
      // The slider of the refresh rate shows the rate that is used while it is known, which is not the value it was created with
      UpdateRefreshRateUI();
      break;
    default:
      break;
    }
  }


  void FramePacingShared::ConfigurationChanged(const DemoWindowMetrics& windowMetrics)
  {
    m_windowSizePx = windowMetrics.GetSizePx();
  }


  void FramePacingShared::Update(const DemoTime& demoTime)
  {
    m_updateTime = demoTime;
    m_frameStarted = false;
    {
      // Before the controls are read, so what a held key changed is used by this frame
      const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.KeyboardMenu);
      m_keyboardMenu.Update(demoTime.ElapsedTime);
    }
    {
      const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.PacerUpdate);
      UpdatePacer();
    }
    {
      const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.TierUpdate);
      UpdateTier();
    }
    if (m_presentMethod == SamplePresentMethod::SwapInterval)
    {
      // The frame starts here: the host updates the app right after the swap of the previous frame, which waited for the display
      StartFrame();
    }

    {
      const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.StatsUI);
      UpdateStatsVisibility();
      UpdateSyncMarker();
      UpdatePacerStats();
      UpdateMarkerStats();
    }
    {
      // The frames everything is known about by now: the GPU time of a frame comes a frame or more after the frame ended, so the
      // chart is that far behind
      const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.WorkChart);
      SampleFrameWorkRecord work;
      while (m_frameWork.TryPop(work))
      {
        m_frameWorkAverage.Add(work);
        if (m_ui.SwitchWorkChart->IsChecked())
        {
          UI::ChartDataEntry entry;
          entry.Values[WorkChartCpuChannel] = ToChartMicroseconds(work.CpuTime);
          entry.Values[WorkChartGpuChannel] = ToChartMicroseconds(work.GpuTime);
          entry.Values[WorkChartFrameChannel] = ToChartMicroseconds(work.FrameTime);
          m_workChartData->Append(entry);
        }
      }
      const TickCount now = m_timer.GetTimestamp();
      if (m_frameWorkAverage.Count() > 0u && (now - m_workLegendTime) >= ChartLegendInterval)
      {
        // The legend says what the lines are at: the average of the last frames, written a few times a second so it can be read
        m_workLegendTime = now;
        const SampleFrameWorkRecord average = m_frameWorkAverage.CalcAverage();
        SetFormattedContent(*m_ui.LabelWorkFrame, "Frame {:.2f} ms", average.FrameTime.TotalMilliseconds());
        if (average.GpuTime.Ticks() > 0)
        {
          SetFormattedContent(*m_ui.LabelWorkGpu, "GPU {:.2f} ms", average.GpuTime.TotalMilliseconds());
        }
        else
        {
          m_ui.LabelWorkGpu->SetContent("GPU not measured");
        }
        SetFormattedContent(*m_ui.LabelWorkCpu, "CPU {:.2f} ms", average.CpuTime.TotalMilliseconds());
      }
    }
    {
      const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.AnimationError);
      UpdateAnimationError();
    }
    if (m_framePacing)
    {
      const int64_t measuredTenths = m_framePacing->GetRunMeasuredTime().Ticks() / (TimeSpan::TicksPerMillisecond * 100);
      if (m_framePacing->GetRunState() != m_cachedRunState || m_framePacing->GetRunId() != m_cachedRunId || measuredTenths != m_cachedMeasuredTenths)
      {
        const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.RunUI);
        UpdateUI();
      }
    }
  }


  void FramePacingShared::Draw()
  {
    const ScopedTraceZone traceZoneDraw(m_trace.get(), m_traceZones.SampleDraw);
    // SamplePresentMethod::WaitThenPresent: the frame starts here, after the host waited for a free buffer
    StartFrame();

    if (m_trace)
    {
      // A frame has its row in the log from the draw on. A frame that starts in the update (SamplePresentMethod::SwapInterval) starts
      // before that, and what is written then lands in the row of the frame before. So what the frame started with is written here.
      m_logFrames[m_frameId % m_logFrames.size()] = {m_frameId, m_trace->GetFrameIndex()};
      if (m_frameStartLogPending)
      {
        m_frameStartLogPending = false;
        LogFrameStart(m_frameStartLogWaitTime);
      }
    }

    if (m_ui.SwitchTestPattern->IsChecked())
    {
      // Use the exact time the frame is animated for, this is what the marker reports
      const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.DrawAnimation);
      DrawAnimation(m_animationTime.TotalSeconds());
    }
    if (m_ui.SwitchBoxAnimation->IsChecked())
    {
      // The same time as the test pattern, drawn on top of it
      const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.DrawBoxAnimation);
      DrawBoxAnimation(m_animationTime.TotalSeconds());
    }

    m_uiExtension->Draw();

    // Tell the marker what the frame was paced by. Until the frame pacer was used the animation time is the time of the framework,
    // which the marker reports by itself.
    if (m_framePacing && (m_pacer || m_animationOffset.TotalNanoseconds() != 0))
    {
      // The marker carries nanoseconds: the times of the pacer go to it as the pacer gave them, and the ones that were read on the clock
      // of the framework are whole ticks of 100ns
      FramePacingFrameSchedule frameSchedule;
      frameSchedule.AnimationTime = m_animationTime;
      if (m_pacer)
      {
        frameSchedule.CpuStartTime = NanosecondTickCountUtil::FromTickCount(m_frameStartTime);
        frameSchedule.IntendedDisplayTime = m_schedule.IntendedDisplayTime;
        frameSchedule.TargetFrameTime = m_schedule.TargetFrameTime;
        frameSchedule.PreferredFrameTime = m_schedule.PreferredFrameTime;
        // The CPU busy time of the marker is the pacer's to count. The marker is drawn after this, so this is the last thing of the
        // draw: what is left of the frame is the host drawing the marker.
        const TimeSpan cpuBusy = m_pacer->GetCpuBusyAt(m_timer.GetTimestamp());
        if (cpuBusy.Ticks() > 0)
        {
          frameSchedule.CpuBusyTime = NanosecondTimeSpanUtil::FromTimeSpan(cpuBusy);
        }
      }
      m_framePacing->SetFrameSchedule(frameSchedule);
    }
  }


  void FramePacingShared::SetMeasurementSupport(const bool presentTimingSupported, const bool gpuTimelineSupported)
  {
    m_presentTimingSupported = presentTimingSupported;
    m_gpuTimelineSupported = gpuTimelineSupported;
    m_ui.SwitchPresentTiming->SetEnabled(presentTimingSupported);
    m_ui.SwitchGpuTimeline->SetEnabled(gpuTimelineSupported);
  }


  void FramePacingShared::SetPresentSchedulingSupport(const bool supported)
  {
    m_presentSchedulingSupported = supported;
  }


  SamplePacerCapabilities FramePacingShared::GetPacerCapabilities() const noexcept
  {
    SamplePacerCapabilities has;
    has.PresentSwapIntervalMax = m_presentMethod == SamplePresentMethod::SwapInterval ? m_presentSwapIntervalMax : 0u;
    has.PresentAfterDuration = m_presentSchedulingSupported;
    // The vertical blanks of a display that refreshes at a variable rate follow the frames, so there they can not hold one
    has.VBlankTimes = m_vsyncTime.TotalNanoseconds() > 0 && m_vsyncPeriod.TotalNanoseconds() > 0 && !m_variableRefreshSeen;
    has.WaitForPresent = m_presentWaitingPresents > 0u;
    has.DisplayTimes = m_presentTimingSupported;
    has.WaitForGpuWork = m_gpuWaitSupported;
    return has;
  }


  bool FramePacingShared::IsTimedPresentRequested() const
  {
    return m_presentSchedulingSupported && m_ui.SwitchTimedPresent->IsChecked();
  }


  void FramePacingShared::SetRequestedKind(const SamplePacerKind kind) noexcept
  {
    m_pacerKind = kind;
  }


  void FramePacingShared::ReadTierRadioButtons()
  {
    if (!m_shownKind.has_value())
    {
      return;
    }
    // A radio button that is checked and is not the one the sample checked: the user asks for that pacer
    const SamplePacerKind shownKind = *m_shownKind;
    for (std::size_t i = 0; i < m_ui.RadioTiers.size(); ++i)
    {
      const SamplePacerKind kind = SamplePacerTierChoiceUtil::ToKind(SamplePacerTierChoiceUtil::Tiers[i]);
      if (kind != shownKind && m_ui.RadioTiers[i]->IsChecked())
      {
        SetRequestedKind(kind);
      }
    }
  }


  void FramePacingShared::UpdateTierChoice()
  {
    // What a capability is worth is the rule of the pacer library, so each question is put to it
    const SamplePacerCapabilities has = GetPacerCapabilities();

    // A tier can be used where the app has what its pacer uses: the library rates what a run with that pacer would use
    for (std::size_t i = 0; i < m_tierOffered.size(); ++i)
    {
      const SamplePacerTier tier = SamplePacerTierChoiceUtil::Tiers[i];
      const SamplePacerKind kind = SamplePacerTierChoiceUtil::ToKind(tier);
      m_tierOffered[i] = SamplePacer::Rate(SamplePacerTierChoiceUtil::ToUsedCapabilities(has, kind)).Tier == tier;
    }

    // What the run is to be paced with: the kind that was asked for where the app has what it uses, else the kind of what is left
    // of it
    m_selectedKind = SamplePacerTierChoiceUtil::ToKind(SamplePacer::Rate(SamplePacerTierChoiceUtil::ToUsedCapabilities(has, m_pacerKind)).Tier);
  }


  void FramePacingShared::ShowTiers(const SamplePacerKind kind)
  {
    // A tier the system can not do is shown as disabled, like every other control that can not be used. The one of the pacer the
    // run is paced by is never disabled.
    const bool isSupported = SamplePacer::IsSupported();
    uint32_t shownIndex = 0;
    for (std::size_t i = 0; i < m_ui.RadioTiers.size(); ++i)
    {
      const SamplePacerKind tierKind = SamplePacerTierChoiceUtil::ToKind(SamplePacerTierChoiceUtil::Tiers[i]);
      UI::RadioButton& rRadioButton = *m_ui.RadioTiers[i];
      const bool isEnabled = isSupported && (tierKind == kind || m_tierOffered[i]);
      if (rRadioButton.IsEnabled() != isEnabled)
      {
        rRadioButton.SetEnabled(isEnabled);
      }
      if (tierKind == kind)
      {
        shownIndex = static_cast<uint32_t>(i);
        if (!rRadioButton.IsChecked())
        {
          rRadioButton.SetIsChecked(true);
        }
      }
    }
    // What the pacer that is checked does, in a line of the pacer library and in the color of its tier. The time the sample can
    // give a present (the switch 'Timed present') changes no tier, so it is the same line with it.
    m_ui.LabelTierDescription->SetSelectedIndex(shownIndex);
    m_ui.LabelTierDescription->SetFontColor(shownIndex < TierColors.size() ? TierColors[shownIndex] : m_ui.DefaultFontColor);
    m_shownKind = kind;
  }


  bool FramePacingShared::IsPresentTimingWanted() const
  {
    return m_presentTimingSupported && m_ui.SwitchPresentTiming->IsChecked();
  }


  bool FramePacingShared::IsGpuTimelineWanted() const
  {
    return m_gpuTimelineSupported && m_ui.SwitchGpuTimeline->IsChecked();
  }


  void FramePacingShared::SetPresentFeedback(const bool enabled, const TimeSpan refreshDuration)
  {
    if (!enabled && m_presentFeedbackEnabled)
    {
      // The frames that are remembered will not be measured anymore
      m_presentFeedback.ClearFrames();
      m_animationError.Clear();
    }
    m_presentFeedbackEnabled = enabled;
    m_measuredRefreshDuration = enabled ? refreshDuration : TimeSpan();
  }


  void FramePacingShared::SetFramePresentId(const uint64_t presentId)
  {
    // The display time is only something the frame pacer aims for, and with present feedback it is not known before a display time arrived
    const bool hasIntendedDisplayTime = m_pacer && m_schedule.IntendedDisplayTime.TotalNanoseconds() != 0;
    m_presentFeedback.AddFrame(presentId, m_frameStartTime,
                               hasIntendedDisplayTime ? std::optional<TickCount>(NanosecondTickCountUtil::ToTickCount(m_schedule.IntendedDisplayTime))
                                                      : std::nullopt);
    if (m_presentFeedbackEnabled)
    {
      // The moment the frame is drawn for, to be held against the time it is shown at
      m_animationError.AddFrame(presentId, NanosecondTimeSpanUtil::ToTimeSpan(m_animationTime), GetTargetFrameTime(),
                                NanosecondTimeSpanUtil::ToTimeSpan(GetRefreshPeriod()));
    }

    {    // So the frame pacer can be told about the frame when its display time arrives, a few frames from now
      PacerPresentFrame& rPresentFrame = m_pacerPresentFrames[presentId % m_pacerPresentFrames.size()];
      rPresentFrame = {};
      rPresentFrame.PresentId = presentId;
      rPresentFrame.PacerFrameId = m_pacer ? m_schedule.FrameId : 0u;
      if (m_pacer)
      {
        // And the other way: a pacer that names a present to wait for does it by the id it gave the frame
        m_pacerFramePresents[m_schedule.FrameId % m_pacerFramePresents.size()] = {m_schedule.FrameId, presentId};
      }
      if (m_trace)
      {
        rPresentFrame.LogFrameIndex = m_trace->GetFrameIndex();
        rPresentFrame.HasLogFrame = true;
      }
    }

    // So something that is measured later about this present (when the GPU worked on it) finds the frame of the sample
    m_presentFrames[presentId % m_presentFrames.size()] = {presentId, m_frameId};
    if (m_trace)
    {
      // And the frame of the log
      m_logPresentFrames[presentId % m_logPresentFrames.size()] = {presentId, m_trace->GetFrameIndex()};
    }
  }


  void FramePacingShared::AddPresentTiming(const uint64_t presentId, const std::optional<TickCount> displayTime,
                                           const std::optional<TickCount> queueOperationsEndTime, const bool isComplete)
  {
    m_presentFeedback.AddPresentTiming(presentId, displayTime, queueOperationsEndTime);
    if (displayTime.has_value())
    {
      m_animationError.AddDisplayTime(presentId, displayTime.value());
    }
    else if (isComplete)
    {
      m_animationError.AddNotShown(presentId);
    }

    // The frame pacer is told when the frame was shown, where the run gives it the display times: it counts the animation error
    // from them. A present without a display time is not reported: no display time is not "never shown".
    if (m_pacer && m_pacerConfig.PresentFeedback && displayTime.has_value())
    {
      const PacerPresentFrame& presentFrame = m_pacerPresentFrames[presentId % m_pacerPresentFrames.size()];
      if (presentFrame.PresentId == presentId && presentFrame.PacerFrameId != 0u)
      {
        m_pacer->AddDisplayTime(presentFrame.PacerFrameId, displayTime.value());
        if (m_trace && presentFrame.HasLogFrame)
        {
          // What the frame pacer was given about the frame
          m_trace->SetValueAt(presentFrame.LogFrameIndex, m_logColumns.FeedbackDisplay, displayTime.value());
        }
      }
    }
  }


  void FramePacingShared::AddSystemWait(const SamplePacerSystemWait kind, const TickCount beginTime, const TickCount endTime)
  {
    // The pacer is told before it is told that the frame starts (StartFrame, which takes the start time of the frame after these
    // waits). A wait that is given when the frame has started would be taken for one of the next frame, so it is left out.
    if (m_pacer && m_pacerSystemWaits && !m_frameStarted && beginTime.Ticks() != 0 && endTime.Ticks() != 0)
    {
      m_pacer->AddSystemWait(kind, beginTime, endTime);
    }
  }


  SamplePresentWaitRequest FramePacingShared::GetPresentWaitRequest()
  {
    SamplePresentWaitRequest request;
    if (m_frameStarted || !IsPacerEnabled())
    {
      return request;
    }
    PlanFrameStart();
    const uint64_t pacerFrameId = m_frameStartPlan.WaitForPresentFrameId;
    if (m_presentWaitRequested || pacerFrameId == 0u)
    {
      return request;
    }
    m_presentWaitRequested = true;
    const PacerFramePresent& framePresent = m_pacerFramePresents[pacerFrameId % m_pacerFramePresents.size()];
    if (framePresent.PacerFrameId == pacerFrameId && framePresent.PresentId != 0u)
    {
      request.PresentId = framePresent.PresentId;
      request.Timeout = m_frameStartPlan.WaitForPresentTimeout;
    }
    return request;
  }


  void FramePacingShared::AddPresentWait(const uint64_t presentId, const TickCount beginTime, const TickCount endTime, const bool shown)
  {
    const uint64_t pacerFrameId = m_frameStartPlan.WaitForPresentFrameId;
    if (!IsPacerEnabled() || !m_frameStartPlanned || pacerFrameId == 0u)
    {
      return;
    }
    const PacerFramePresent& framePresent = m_pacerFramePresents[pacerFrameId % m_pacerFramePresents.size()];
    if (framePresent.PacerFrameId != pacerFrameId || framePresent.PresentId != presentId)
    {
      // Not the present the frame pacer asked for
      return;
    }
    SamplePacerPresentWaitReport report;
    report.FrameId = pacerFrameId;
    report.BeginTime = beginTime;
    report.EndTime = endTime;
    report.Shown = shown;
    m_pacer->AddPresentWait(report);
    // The frame is planned again: the wait can have taken long, and the time to wait until can have moved with what the wait told
    // the pacer. The present that was waited for is not asked for again.
    m_frameStartPlan = m_pacer->PlanFrame(m_timer.GetTimestamp());
  }


  SampleGpuWaitRequest FramePacingShared::GetGpuWaitRequest()
  {
    SampleGpuWaitRequest request;
    if (m_frameStarted || !IsPacerEnabled())
    {
      return request;
    }
    PlanFrameStart();
    const uint64_t pacerFrameId = m_frameStartPlan.WaitForGpuWorkFrameId;
    if (m_gpuWaitRequested || pacerFrameId == 0u)
    {
      return request;
    }
    m_gpuWaitRequested = true;
    const PacerFramePresent& framePresent = m_pacerFramePresents[pacerFrameId % m_pacerFramePresents.size()];
    if (framePresent.PacerFrameId == pacerFrameId && framePresent.PresentId != 0u)
    {
      request.PresentId = framePresent.PresentId;
      request.Timeout = m_frameStartPlan.WaitForGpuWorkTimeout;
    }
    return request;
  }


  void FramePacingShared::AddGpuWait(const uint64_t presentId, const TickCount beginTime, const TickCount endTime, const bool done)
  {
    const uint64_t pacerFrameId = m_frameStartPlan.WaitForGpuWorkFrameId;
    if (!IsPacerEnabled() || !m_frameStartPlanned || pacerFrameId == 0u)
    {
      return;
    }
    const PacerFramePresent& framePresent = m_pacerFramePresents[pacerFrameId % m_pacerFramePresents.size()];
    if (framePresent.PacerFrameId != pacerFrameId || framePresent.PresentId != presentId)
    {
      // Not the frame the frame pacer asked for
      return;
    }
    SamplePacerGpuWaitReport report;
    report.FrameId = pacerFrameId;
    report.BeginTime = beginTime;
    report.EndTime = endTime;
    report.Done = done;
    m_pacer->AddGpuWait(report);
    // The frame is planned again, as after a wait for a present. The frame that was waited for is not asked for again.
    m_frameStartPlan = m_pacer->PlanFrame(m_timer.GetTimestamp());
  }


  void FramePacingShared::AddPresentCall(const uint64_t presentId, const TickCount callTime, const TickCount returnTime, const bool accepted)
  {
    if (!IsPacerEnabled() || presentId == 0u || presentId == m_reportedPresentId)
    {
      return;
    }
    m_reportedPresentId = presentId;
    const PacerPresentFrame& presentFrame = m_pacerPresentFrames[presentId % m_pacerPresentFrames.size()];
    if (presentFrame.PresentId == presentId && presentFrame.PacerFrameId != 0u)
    {
      ReportPresent(presentFrame.PacerFrameId, callTime, returnTime, accepted);
    }
  }


  void FramePacingShared::ReportPresent(const uint64_t pacerFrameId, const TickCount callTime, const TickCount returnTime, const bool accepted)
  {
    if (pacerFrameId == 0u)
    {
      return;
    }
    SamplePacerPresentReport report;
    report.FrameId = pacerFrameId;
    report.CallTime = callTime;
    report.ReturnTime = returnTime;
    report.Accepted = accepted;
    m_pacer->AddPresent(report);
  }


  void FramePacingShared::OnSwapchainRecreated()
  {
    if (!IsPacerEnabled())
    {
      return;
    }
    // The presents of the swapchain before are gone: none can be waited for, and what queued up below it is not there anymore.
    // The frame that is about to start is planned again.
    m_pacer->ForgetPresents();
    m_frameStartPlanned = false;
    if (m_trace)
    {
      m_trace->AddEvent("pacerForgetPresents", "reason=swapchain");
    }
  }


  void FramePacingShared::PlanFrameStart()
  {
    if (m_frameStartPlanned)
    {
      return;
    }
    m_frameStartPlanned = true;
    m_presentWaitRequested = false;
    m_gpuWaitRequested = false;
    if (m_presentMethod == SamplePresentMethod::SwapInterval && m_framePacing && m_presentPlan.FrameId != m_reportedPresentFrameId)
    {
      // The host presents the frames of the app (a swap), so the frame pacing service is the one that knows when
      TickCount swapCallTime;
      TickCount swapReturnTime;
      if (m_framePacing->TryGetLastSwapTimes(swapCallTime, swapReturnTime))
      {
        m_reportedPresentFrameId = m_presentPlan.FrameId;
        ReportPresent(m_presentPlan.FrameId, swapCallTime, swapReturnTime, true);
      }
    }
    if (m_pacer && IsVBlankKind(m_pacerConfig.Kind) && m_vsyncTime.TotalNanoseconds() > 0 && m_vsyncPeriod.TotalNanoseconds() > 0 &&
        !m_variableRefreshSeen && m_vsyncTime != m_pacerVBlankTime)
    {
      // Where the refreshes of the display are, before the frame is planned. Only a new one is given: a vertical blank the pacer
      // has tells it nothing, whenever it is read again. It is the one of the display the window is on (NativeWindowVSyncInfo).
      m_pacerVBlankTime = m_vsyncTime;
      m_pacer->AddVBlank(m_vsyncTime, m_vsyncPeriod, m_vsyncReadTime);
    }
    const TickCount now = m_timer.GetTimestamp();
    if (m_frameStartWaitTime.Ticks() == 0)
    {
      m_frameStartWaitTime = now;
    }
    m_frameStartPlan = m_pacer->PlanFrame(now);
  }


  void FramePacingShared::AddGpuInterval(const uint64_t presentId, const TickCount gpuStartTime, const TickCount gpuEndTime)
  {
    m_presentFeedback.AddGpuInterval(presentId, gpuStartTime, gpuEndTime);
    if (IsPacerEnabled())
    {
      // The pacer is given when the GPU worked on the frame. How long it worked on the same frame can have been given already
      // (AddGpuTime): the pacer lets a later report for a frame take the place of the first.
      const PacerPresentFrame& pacerFrame = m_pacerPresentFrames[presentId % m_pacerPresentFrames.size()];
      if (pacerFrame.PresentId == presentId && pacerFrame.PacerFrameId != 0u)
      {
        SamplePacerGpuWorkReport report;
        report.FrameId = pacerFrame.PacerFrameId;
        report.BeginTime = gpuStartTime;
        report.EndTime = gpuEndTime;
        report.Duration = gpuEndTime - gpuStartTime;
        m_pacer->AddGpuWork(report);
      }
    }
    {
      const PresentFrame& presentFrame = m_presentFrames[presentId % m_presentFrames.size()];
      if (presentFrame.PresentId == presentId && presentFrame.FrameId != 0u)
      {
        m_frameWork.AddGpuInterval(presentFrame.FrameId, gpuStartTime, gpuEndTime);
      }
    }
    if (m_trace)
    {
      const LogPresentFrame& presentFrame = m_logPresentFrames[presentId % m_logPresentFrames.size()];
      if (presentFrame.PresentId == presentId)
      {
        m_trace->SetValueAt(presentFrame.FrameIndex, m_logColumns.GpuWorkBegin, gpuStartTime);
        m_trace->SetValueAt(presentFrame.FrameIndex, m_logColumns.GpuWorkEnd, gpuEndTime);
      }
    }
  }


  void FramePacingShared::AddGpuTime(const uint64_t frameId, const TimeSpan gpuTime, const std::optional<TickCount> gpuEndTime)
  {
    if (IsPacerEnabled())
    {
      // The work of the GPU on a earlier frame, as far as the app has measured it: the pacer puts it together with the work of the
      // CPU. A app that also knows when the GPU began says so afterwards (AddGpuInterval).
      const PacerFrame& pacerFrame = m_pacerFrames[frameId % m_pacerFrames.size()];
      if (frameId != 0u && pacerFrame.FrameId == frameId && pacerFrame.PacerFrameId != 0u)
      {
        SamplePacerGpuWorkReport report;
        report.FrameId = pacerFrame.PacerFrameId;
        report.EndTime = gpuEndTime;
        report.Duration = gpuTime;
        m_pacer->AddGpuWork(report);
      }
    }
    m_frameWork.AddGpuTime(frameId, gpuTime, gpuEndTime);
    m_gpuTimeLogFrameIndex.reset();
    if (m_trace)
    {
      const LogFrame& logFrame = m_logFrames[frameId % m_logFrames.size()];
      if (frameId != 0u && logFrame.FrameId == frameId)
      {
        // The time is written to the frame it was measured on, which is a frame or more before the one that is being drawn
        m_trace->SetValueAt(logFrame.FrameIndex, m_logColumns.GpuTime, gpuTime);
        if (gpuEndTime.has_value())
        {
          m_trace->SetValueAt(logFrame.FrameIndex, m_logColumns.GpuWorkEnd, gpuEndTime.value());
        }
        m_gpuTimeLogFrameIndex = logFrame.FrameIndex;
      }
    }
  }


  void FramePacingShared::AddGpuClockCalibration(const TimeSpan readTime)
  {
    if (m_trace)
    {
      m_trace->AddEvent("gpuClockCalibration", fmt::format("readTicks={};maxDeviationTicks={}", readTime.Ticks(), (readTime.Ticks() + 1) / 2));
    }
  }


  void FramePacingShared::AddGpuClockCalibration(const TimeSpan readTime, const TimeSpan maxDeviation,
                                                 const std::optional<double> clockRateDeviationPpm)
  {
    if (m_trace)
    {
      // The rate is left out until it was measured: the period the device states is used until then
      m_trace->AddEvent("gpuClockCalibration", clockRateDeviationPpm.has_value()
                                                 ? fmt::format("readTicks={};maxDeviationTicks={};clockRateDeviationPpm={:.2f}", readTime.Ticks(),
                                                               maxDeviation.Ticks(), clockRateDeviationPpm.value())
                                                 : fmt::format("readTicks={};maxDeviationTicks={}", readTime.Ticks(), maxDeviation.Ticks()));
    }
  }


  void FramePacingShared::MarkFlush()
  {
    if (m_trace)
    {
      m_trace->SetValue(m_logColumns.FlushCall, m_timer.GetTimestamp());
    }
  }


  RaymarchScene FramePacingShared::GetBackgroundScene() const
  {
    if (m_ui.RadioBackgroundMandelbrot->IsChecked())
    {
      return RaymarchScene::Mandelbrot;
    }
    if (m_ui.RadioBackgroundBlobs->IsChecked())
    {
      return RaymarchScene::Blobs;
    }
    if (m_ui.RadioBackgroundLace->IsChecked())
    {
      return RaymarchScene::Lace;
    }
    return m_ui.RadioBackgroundHall->IsChecked() ? RaymarchScene::Hall : RaymarchScene::Flight;
  }


  RaymarchParams FramePacingShared::GetRaymarchParams()
  {
    // The background is animated for the animation time of the frame, so the frame has to be started
    StartFrame();

    const double animationSeconds = m_animationTime.TotalSeconds();
    RaymarchParams params;
    params.Steps = m_ui.SliderGpuLoad->GetValue();
    params.Scene = GetBackgroundScene();
    params.TravelPhase = ToPhase(animationSeconds, LocalConfig::TravelSeconds);
    params.SwayPhase = ToPhase(animationSeconds, LocalConfig::SwaySeconds);
    params.MorphPhase = ToPhase(animationSeconds, LocalConfig::MorphSeconds);
    return params;
  }


  void FramePacingShared::EndFrame(const TimeSpan gpuTime)
  {
    const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.EndFrame);
    const TickCount now = m_timer.GetTimestamp();
    m_lastCpuTime = now - m_frameStartTime;
    m_lastGpuTime = TimeSpan(std::max(gpuTime.Ticks(), int64_t{0}));
    m_lastPresentWait = {};
    m_frameWork.EndCpuWork(now);
    m_tierFactsReady = true;
    if (m_pacer)
    {
      // The work of the CPU ends here, and the frame pacer says how the frame is to be presented
      m_presentPlan = m_pacer->EndFrame(now);
    }
    if (m_trace)
    {
      // What the frame pacer is told about the frame
      m_trace->SetValue(m_logColumns.EndFrame, now);
      m_trace->SetValue(m_logColumns.WorkCpu, m_lastCpuTime);
      m_trace->SetValue(m_logColumns.WorkGpu, m_lastGpuTime);
      if (m_lastGpuTime.Ticks() > 0 && m_gpuTimeLogFrameIndex.has_value())
      {
        // The GPU time is the one of an earlier frame, so the row says which
        m_trace->SetUInt64(m_logColumns.WorkGpuFrameIndex, m_gpuTimeLogFrameIndex.value().Value);
      }
    }
  }


  void FramePacingShared::WaitForPresent()
  {
    const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.WaitForPresent);
    m_presentRelativeTarget = {};
    if (!m_pacer)
    {
      return;
    }
    // The frame pacer says when the frame is presented: the sample waits until the time it is given and computes none
    if (m_presentPlan.PresentTime.Ticks() != 0)
    {
      const TickCount waitStartTime = m_timer.GetTimestamp();
      WaitUntil(m_presentPlan.PresentTime, m_traceZones.HoldBeforePresent);
      const TickCount waitEndTime = m_timer.GetTimestamp();
      m_lastPresentWait = waitEndTime - waitStartTime;
      LogPresentWait(waitStartTime, m_presentPlan.PresentTime, waitEndTime);
    }
    if (m_presentPlan.MinimumDuration.TotalNanoseconds() > 0)
    {
      // The pacer gives the present a time: the frame before this one stays on screen at least that long. It is given next to
      // the wait above, which the pacer still asks for.
      m_presentRelativeTarget = NanosecondTimeSpanUtil::ToTimeSpan(m_presentPlan.MinimumDuration);
      if (m_trace)
      {
        m_trace->SetValue(m_logColumns.PresentTarget, m_presentRelativeTarget);
      }
    }
  }


  void FramePacingShared::LogPresentWait(const TickCount beginTime, const TickCount targetTime, const TickCount endTime)
  {
    if (m_trace)
    {
      // A wait is logged with the time it aimed at and the time it woke, so how far it overslept is a value
      m_trace->SetValue(m_logColumns.PresentWait, endTime - beginTime);
      m_trace->SetValue(m_logColumns.PresentWaitBegin, beginTime);
      m_trace->SetValue(m_logColumns.PresentWaitTarget, targetTime);
      m_trace->SetValue(m_logColumns.PresentWaitEnd, endTime);
    }
  }


  void FramePacingShared::WaitUntil(const TickCount time, const TraceZone zone) const
  {
    // A wait for a time that has passed is not a zone
    const ScopedTraceZone traceZone(m_trace.get(), (time - m_timer.GetTimestamp()).Ticks() > 0 ? zone : TraceZone());
    for (;;)
    {
      const TimeSpan remaining = time - m_timer.GetTimestamp();
      if (remaining.Ticks() <= 0)
      {
        return;
      }
      if (remaining > LocalConfig::CoarseSleepThreshold)
      {
        std::this_thread::sleep_for(std::chrono::microseconds((remaining - LocalConfig::CoarseSleepMargin).Ticks() / TimeSpan::TicksPerMicrosecond));
      }
      else
      {
        std::this_thread::yield();
      }
    }
  }


  void FramePacingShared::ToggleRun()
  {
    if (!m_framePacing)
    {
      return;
    }
    if (m_framePacing->GetRunState() == FramePacingRunState::Idle)
    {
      m_framePacing->BeginRun(StringViewLite(m_runName), TimeSpan());
    }
    else
    {
      m_framePacing->EndRun();
    }
    UpdateUI();
  }


  void FramePacingShared::StartTimedRun()
  {
    if (!m_framePacing || m_framePacing->GetRunState() != FramePacingRunState::Idle)
    {
      return;
    }
    const int32_t seconds = m_ui.SliderDuration->GetValue();
    const std::string runName = fmt::format("{} {}s", m_runName, seconds);
    m_framePacing->BeginRun(StringViewLite(runName), TimeSpan::FromSeconds(seconds));
    UpdateUI();
  }


  void FramePacingShared::UpdateUI()
  {
    if (!m_framePacing)
    {
      return;
    }
    m_cachedRunState = m_framePacing->GetRunState();
    m_cachedRunId = m_framePacing->GetRunId();
    const TimeSpan measuredTime = m_framePacing->GetRunMeasuredTime();
    m_cachedMeasuredTenths = measuredTime.Ticks() / (TimeSpan::TicksPerMillisecond * 100);

    const bool isIdle = m_cachedRunState == FramePacingRunState::Idle;
    const TimeSpan duration = m_framePacing->GetRunDuration();
    if (m_cachedRunState == FramePacingRunState::Measuring)
    {
      const double measuredSeconds = static_cast<double>(m_cachedMeasuredTenths) / 10.0;
      m_ui.LabelRun->SetContent(duration > TimeSpan() ? fmt::format("Run {}: measuring {:.1f} / {} s", m_cachedRunId, measuredSeconds,
                                                                    duration.Ticks() / TimeSpan::TicksPerSecond)
                                                      : fmt::format("Run {}: measuring {:.1f} s", m_cachedRunId, measuredSeconds));
    }
    else
    {
      m_ui.LabelRun->SetContent(fmt::format("Run {}: {}", m_cachedRunId, ToString(m_cachedRunState)));
    }
    m_ui.ButtonRun->SetContent(isIdle ? "Start run" : "End run");
    m_ui.ButtonTimedRun->SetEnabled(isIdle);
    m_ui.SliderDuration->SetEnabled(isIdle);
  }


  void FramePacingShared::UpdatePacer()
  {
    if (UpdateDetectedRefreshPeriod())
    {
      UpdateRefreshRateUI();
    }

    ReadTierRadioButtons();
    if (m_kindChangeFrames > 0u && m_pacer)
    {
      // A run that measures what a change of the pacer does: the next pacer of the order, every so many paced frames
      ++m_kindChangeFrameCount;
      if (m_kindChangeFrameCount >= m_kindChangeFrames)
      {
        m_kindChangeFrameCount = 0;
        m_kindChangeIndex = (m_kindChangeIndex + 1u) % SamplePacerTierChoiceUtil::KindChangeOrder.size();
        SetRequestedKind(SamplePacerTierChoiceUtil::KindChangeOrder[m_kindChangeIndex]);
      }
    }
    {    // When the display refreshes according to the window system, read once per frame
      const auto window = m_window.lock();
      const NativeWindowVSyncInfo vsyncInfo = window ? window->TryGetVSyncInfo() : NativeWindowVSyncInfo();
      m_vsyncReadTime = m_timer.GetTimestamp();
      m_vsyncTime = vsyncInfo.IsValid() ? vsyncInfo.VSyncTime : NanosecondTickCount();
      m_vsyncPeriod = vsyncInfo.IsValid() ? vsyncInfo.RefreshPeriod : NanosecondTimeSpan();

      // Variable refresh: once it was seen it stays seen for this refresh rate
      if (m_detectedRefreshRateHz != m_variableRefreshSeenRateHz)
      {
        m_variableRefreshSeenRateHz = m_detectedRefreshRateHz;
        m_variableRefreshSeen = false;
      }
      if (m_swapchainRefresh == SampleSwapchainRefresh::Variable || (window && window->TryGetVariableRefreshInfo().IsSeen()))
      {
        m_variableRefreshSeen = true;
      }
    }
    UpdateTierChoice();

    SamplePacerConfig pacerConfig;
    pacerConfig.RefreshPeriod = GetRefreshPeriod();
    pacerConfig.TargetFps = static_cast<uint32_t>(std::max(m_ui.SliderTargetFps->GetValue(), 0));
    pacerConfig.Adaptive = m_ui.SwitchAdaptive->IsChecked();
    // The kind the app has what it takes for right now (UpdateTierChoice). The pacer is told what the app has: the kind is what of
    // that it uses, and the present that takes a time and the wait for the GPU's work where they were asked for.
    pacerConfig.Kind = m_selectedKind;
    pacerConfig.Capabilities = GetPacerCapabilities();
    pacerConfig.TimedPresent = IsTimedPresentRequested();
    pacerConfig.GpuWait = m_pacerGpuWait;
    pacerConfig.ReadyPlacePercent = m_readyPlacePercent;
    pacerConfig.SwapChainImages = m_swapchainImageCount;
    pacerConfig.SystemHoldsLoop = m_systemHoldsLoop;
    pacerConfig.Aim = m_ui.SwitchLowLatency->IsChecked() ? SamplePacerAim::LowLatency : SamplePacerAim::Smoothness;
    // The presents that may wait: what the command line said, else the presents back the app can wait for with the pacer that waits
    // for one, else the default of the library
    pacerConfig.WaitingPresents = m_waitingPresentsOption;
    pacerConfig.MaxFramesInFlight = std::max(m_maxFramesInFlight, 1u);
    // The pause after a start is the pacer's to make. The sample only says how long it is to be (--Pacer.Drain).
    pacerConfig.StartupPauseRefreshes = static_cast<uint32_t>(std::max(m_drainRefreshes, 0));

    const bool pacerOn = SamplePacer::IsSupported() && m_ui.SwitchPacer->IsChecked();
    // The settings of the pacer do nothing while it is off, so they can only be changed while it is on
    if (m_ui.SliderTargetFps->IsEnabled() != pacerOn)
    {
      m_ui.SliderTargetFps->SetEnabled(pacerOn);
    }
    if (m_ui.SwitchAdaptive->IsEnabled() != pacerOn)
    {
      m_ui.SwitchAdaptive->SetEnabled(pacerOn);
    }
    if (m_ui.SwitchLowLatency->IsEnabled() != pacerOn)
    {
      m_ui.SwitchLowLatency->SetEnabled(pacerOn);
    }
    // A present that takes a time is the pacer's to use where the app has one
    const bool timedPresentAvailable = pacerOn && m_presentSchedulingSupported;
    if (m_ui.SwitchTimedPresent->IsEnabled() != timedPresentAvailable)
    {
      m_ui.SwitchTimedPresent->SetEnabled(timedPresentAvailable);
    }
    // Present feedback needs the display times of the presents, so it is only offered while the app is asked to measure them and only
    // used while the swapchain does. The pacer counts the animation error from it and paces by none of it.
    const bool feedbackAvailable = pacerOn && IsPresentTimingWanted();
    if (m_ui.SwitchPacerFeedback->IsEnabled() != feedbackAvailable)
    {
      m_ui.SwitchPacerFeedback->SetEnabled(feedbackAvailable);
    }
    pacerConfig.PresentFeedback = feedbackAvailable && m_presentFeedbackEnabled && m_ui.SwitchPacerFeedback->IsChecked();

    if (!pacerOn)
    {
      m_pacer.reset();
      m_pacerChanges = {};
      m_pacerPresentFrames = {};
    }
    else if (!m_pacer)
    {
      m_pacer = std::make_unique<SamplePacer>(pacerConfig);
      // A new pacer counts its frames from one again, so what is remembered about the frames of the one before must not reach it
      m_pacerPresentFrames = {};
      m_pacerFramePresents = {};
      m_pacerFrames = {};
      m_reportedPresentFrameId = 0;
      m_frameStartPlanned = false;
      m_presentPlan = {};
      m_frameStats.Clear();
      m_pacerChanges = {};
      m_pacerChanges.StartTime = m_timer.GetTimestamp();
      // And it has not been told where the refreshes of the display are
      m_pacerVBlankTime = {};
    }
    else if (pacerConfig != m_pacerConfig)
    {
      // Another kind, or a app that has something else now, is the same pacer with another set to use: its frames go on and nothing
      // starts again. With other settings it starts again (a empty frame window, the swap interval of the target frame rate).
      SamplePacerConfig comparedConfig = pacerConfig;
      comparedConfig.Kind = m_pacerConfig.Kind;
      comparedConfig.Capabilities = m_pacerConfig.Capabilities;
      comparedConfig.TimedPresent = m_pacerConfig.TimedPresent;
      comparedConfig.GpuWait = m_pacerConfig.GpuWait;
      // The display times are part of the set the pacer uses
      comparedConfig.PresentFeedback = m_pacerConfig.PresentFeedback;
      const bool startsAgain = comparedConfig != m_pacerConfig;
      m_pacer->SetConfig(pacerConfig);
      m_frameStartPlanned = false;
      if (startsAgain)
      {
        m_frameStats.Clear();
        m_pacerChanges = {};
        m_pacerChanges.StartTime = m_timer.GetTimestamp();
      }
      // The pacer is told where the refreshes are again: with another refresh period the ones it had are not of this display, and a
      // pacer that takes up the vertical blank times has none yet
      m_pacerVBlankTime = {};
    }
    m_pacerConfig = pacerConfig;
  }


  void FramePacingShared::WaitForFrameStart()
  {
    if (m_frameStarted || !m_pacer)
    {
      return;
    }
    // The frame pacer says what the start of the frame waits for
    PlanFrameStart();
    WaitUntil(m_frameStartPlan.StartTime, m_traceZones.WaitForFrameStart);
  }


  void FramePacingShared::StartFrame()
  {
    if (m_frameStarted)
    {
      return;
    }
    m_frameStarted = true;
    ++m_frameId;
    const ScopedTraceZone traceZoneStart(m_trace.get(), m_traceZones.StartFrame);

    if (m_pacer)
    {
      // A app that does not wait before the frame takes anything (WaitForFrameStart) has not asked the frame pacer yet
      PlanFrameStart();
    }
    // The frame waited from where it was planned first (PlanFrameStart), which can have been before this call (WaitForFrameStart)
    const TickCount frameWaitStartTime = m_frameStartWaitTime.Ticks() != 0 ? m_frameStartWaitTime : m_timer.GetTimestamp();
    m_frameStartWaitTime = {};
    // The time the start of the frame is held to, as the frame pacer gave it (zero: it is not held)
    const TickCount frameStartHoldTime = m_pacer ? m_frameStartPlan.StartTime : TickCount();
    m_frameStartLogTargetTime = frameStartHoldTime;
    if (m_pacer)
    {
      // A wait that was made already (WaitForFrameStart) returns at once here
      WaitUntil(frameStartHoldTime, m_traceZones.WaitForFrameStart);
    }
    m_frameStartPlanned = false;

    const TickCount frameStartTime = m_timer.GetTimestamp();
    m_frameInterval = m_frameStartTime.Ticks() != 0 ? (frameStartTime - m_frameStartTime) : TimeSpan();
    m_frameStartTime = frameStartTime;
    m_frameWork.BeginFrame(m_frameId, frameStartTime);

    // The frame that just ended was held for the swap interval of its schedule (one refresh without the pacer)
    if (m_frameInterval.Ticks() > 0 && m_pacerConfig.RefreshPeriod.TotalNanoseconds() > 0)
    {
      const TimeSpan refreshPeriod = NanosecondTimeSpanUtil::ToTimeSpan(m_pacerConfig.RefreshPeriod);
      m_frameStats.AddFrame(frameStartTime, m_frameInterval, refreshPeriod, std::max(m_schedule.SwapInterval, 1u));
    }

    // The animation clock counts nanoseconds, as the steps of the pacer are in them: a step that was rounded to a tick of 100ns would
    // move the animation off the display by the rest of every frame
    const NanosecondTimeSpan frameworkTime = NanosecondTimeSpanUtil::FromTimeSpan(TimeSpan(m_updateTime.CurrentTickCount.Ticks()));
    if (m_pacer)
    {
      m_schedule = m_pacer->BeginFrame(frameStartTime);
      m_pacerFrames[m_frameId % m_pacerFrames.size()] = {m_frameId, m_schedule.FrameId};
      if (m_schedule.Change != SamplePacerChange::Unchanged)
      {
        if (m_schedule.Change == SamplePacerChange::Slower)
        {
          ++m_pacerChanges.SlowerCount;
        }
        else
        {
          ++m_pacerChanges.FasterCount;
        }
        m_pacerChanges.LastChange = m_schedule.Change;
        m_pacerChanges.LastChangeTime = frameStartTime;
      }
      // The frame is animated by the step of the pacer: the refreshes the display moves on. A paused app still stands still.
      if (m_updateTime.ElapsedTime.Ticks() != 0)
      {
        m_animationTime += m_schedule.AnimationStep;
      }
      m_animationOffset = m_animationTime - frameworkTime;
    }
    else
    {
      m_schedule = {};
      m_animationTime = frameworkTime + m_animationOffset;
    }
    // Written to the log once the frame has its row there (Draw)
    m_frameStartLogPending = true;
    m_frameStartLogWaitTime = frameWaitStartTime;

    // The simulated CPU load, and now and then a frame that runs long
    m_frameCpuSpikeMs = (m_cpuSpikeMs > 0 && (m_frameId % static_cast<uint64_t>(std::max(m_cpuSpikeIntervalFrames, 1))) == 0u) ? m_cpuSpikeMs : 0;
    BurnCpu(TimeSpan::FromMilliseconds(static_cast<int64_t>(m_ui.SliderCpuLoad->GetValue()) + m_frameCpuSpikeMs));
  }


  void FramePacingShared::RegisterLogColumns()
  {
    if (!m_trace)
    {
      return;
    }
    ITraceService& rLog = *m_trace;
    LogColumns& rColumns = m_logColumns;
    rColumns.PacerOn = rLog.RegisterValue("pacerOn", TraceUnit::Flag, "1 if the frame pacer of the sample paced the frame");
    rColumns.SwapInterval =
      rLog.RegisterValue("swapInterval", TraceUnit::Count, "The number of display refreshes the frame pacer holds the frame for");
    rColumns.PreferredSwapInterval =
      rLog.RegisterValue("preferredSwapInterval", TraceUnit::Count, "The swap interval of the target frame rate, the pacer never runs faster");
    rColumns.Change = rLog.RegisterValue("pacerChange", TraceUnit::Code,
                                         "What the frame pacer did to the swap interval at this frame: 0 unchanged, 1 slower, 2 faster");
    rColumns.AnimationStep =
      rLog.RegisterValue("animationStepTicks", TraceUnit::DurationTicks, "The step of the frame pacer from the animation time of the frame before");
    rColumns.WindowFrames = rLog.RegisterValue("pacerWindowFrames", TraceUnit::Count, "The frames in the frame window of the frame pacer");
    rColumns.WindowLateFrames =
      rLog.RegisterValue("pacerWindowLateFrames", TraceUnit::Count, "The frames in the frame window the frame pacer counts as late");
    rColumns.WindowStartsAhead =
      rLog.RegisterValue("pacerWindowStartsAheadTicks", TraceUnit::DurationTicks,
                         "How far the frames in the frame window began before the times the frame pacer gave for them, added up over the "
                         "frames that were not late: about zero when the app waits for those times, positive when the loop runs ahead");
    rColumns.WindowAverageWork = rLog.RegisterValue("pacerWindowAverageWorkTicks", TraceUnit::DurationTicks,
                                                    "The average work of the frames in the frame window of the frame pacer");
    rColumns.WindowSpan = rLog.RegisterValue("pacerWindowSpanTicks", TraceUnit::DurationTicks, "The time the frame window of the frame pacer spans");
    rColumns.WindowFull = rLog.RegisterValue("pacerWindowFull", TraceUnit::Flag, "1 if the frame window of the frame pacer is full");
    rColumns.FrameWaitStart = rLog.RegisterValue("frameWaitStartTicks", TraceUnit::Ticks,
                                                 "When the sample began to wait for the start of the frame (it holds the start to the time the frame "
                                                 "before was aimed at)");
    rColumns.FrameWaitTarget = rLog.RegisterValue("frameWaitTargetTicks", TraceUnit::Ticks,
                                                  "The time the start of the frame was held to (empty: it was not held). The frame started at "
                                                  "frameStartTicks, so the difference is how late the wait woke");
    rColumns.FrameStart =
      rLog.RegisterValue("frameStartTicks", TraceUnit::Ticks, "When the sample started the frame, which is the start the frame pacer is given");
    rColumns.EndFrame =
      rLog.RegisterValue("endFrameTicks", TraceUnit::Ticks, "When the work of the frame was done, which is what the frame pacer is told");
    rColumns.WorkCpu = rLog.RegisterValue("workCpuTicks", TraceUnit::DurationTicks, "How long the CPU worked on the frame");
    rColumns.WorkGpu = rLog.RegisterValue("workGpuTicks", TraceUnit::DurationTicks,
                                          "The GPU time the frame pacer was told with the frame: the one of the last frame that was measured");
    rColumns.WorkGpuFrameIndex =
      rLog.RegisterValue("workGpuFrameIndex", TraceUnit::Id,
                         "The frame (frameIndex) the workGpuTicks of the row was measured on: an earlier frame, as the GPU time of a frame "
                         "is not known when the frame ends");
    rColumns.GpuTime = rLog.RegisterValue("gpuTimeTicks", TraceUnit::DurationTicks,
                                          "The GPU time of the frame itself, written when it was measured, a frame or more later (empty: the "
                                          "frame was not measured)");
    rColumns.GpuWorkBegin =
      rLog.RegisterValue("gpuWorkBeginTicks", TraceUnit::Ticks, "When the GPU started on the frame, on the clock of the framework");
    rColumns.GpuWorkEnd = rLog.RegisterValue("gpuWorkEndTicks", TraceUnit::Ticks, "When the GPU finished the frame, on the clock of the framework");
    rColumns.FlushCall = rLog.RegisterValue("flushCallTicks", TraceUnit::Ticks,
                                            "When glFlush was called after the last command of the frame: the GPU is asked to work on the "
                                            "frame from here (only with --GLFlush, without it the driver decides when, the swap at the latest)");
    rColumns.PresentWait = rLog.RegisterValue("presentWaitTicks", TraceUnit::DurationTicks,
                                              "How long the sample delayed the present of the frame, to hold it for its swap interval");
    rColumns.PresentWaitBegin =
      rLog.RegisterValue("presentWaitBeginTicks", TraceUnit::Ticks, "When the sample began to wait with the finished frame before its present");
    rColumns.PresentWaitTarget = rLog.RegisterValue("presentWaitTargetTicks", TraceUnit::Ticks, "The time the wait before the present aimed at");
    rColumns.PresentWaitEnd =
      rLog.RegisterValue("presentWaitEndTicks", TraceUnit::Ticks, "When the wait before the present woke, the present follows");
    rColumns.CpuLoad = rLog.RegisterValue("cpuLoadMs", TraceUnit::Count, "The CPU load setting: the milliseconds the sample is busy per frame");
    rColumns.GpuLoad = rLog.RegisterValue("gpuLoadSteps", TraceUnit::Count,
                                          "The GPU load setting: the steps of the background (for the lace the rounds of detail and the "
                                          "samples per pixel come from it)");
    rColumns.PacerFrameId =
      rLog.RegisterValue("pacerFrameId", TraceUnit::Id, "The id the frame pacer gave the frame, present feedback is given with it");
    rColumns.NextFrameStart = rLog.RegisterValue("nextFrameStartTicks", TraceUnit::Ticks,
                                                 "The start of the frame plus its swap interval according to the frame pacer: what the "
                                                 "waits of the sample hold to");
    rColumns.FeedbackOn =
      rLog.RegisterValue("pacerFeedbackOn", TraceUnit::Flag,
                         "1 if the frame pacer is given the display times of the frames: it counts the animation error from them, it paces "
                         "the same");
    rColumns.FeedbackDisplay =
      rLog.RegisterValue("feedbackDisplayTicks", TraceUnit::Ticks, "The display time of the frame the frame pacer was given as a display report");
    rColumns.DisplayReports = rLog.RegisterValue("pacerDisplayReports", TraceUnit::Count,
                                                 "With pacerFeedbackOn: the display times the pacer took as display "
                                                 "reports, counted since the pacer was made. It counts the animation error from them "
                                                 "and paces by none of it");
    rColumns.DisplayRefused = rLog.RegisterValue("pacerDisplayRefused", TraceUnit::Count,
                                                 "The display times the pacer did not take: for a frame it does not keep (more than 64 "
                                                 "frames old) or not newer than the one before");
    rColumns.DisplayJudgedFrames =
      rLog.RegisterValue("pacerDisplayJudgedFrames", TraceUnit::Count,
                         "The frames the pacer judged from the display reports: the frame and the frame before it both have a display time");
    rColumns.DisplayErrorFrames = rLog.RegisterValue("pacerDisplayErrorFrames", TraceUnit::Count,
                                                     "The judged frames with a animation error of more than a millisecond: their animation "
                                                     "time step less the time between the two display times");
    rColumns.DisplayOffTargetFrames = rLog.RegisterValue("pacerDisplayOffTargetFrames", TraceUnit::Count,
                                                         "The judged frames shown half a refresh or more off where their animation time "
                                                         "step put them: at another refresh");
    rColumns.DisplayStartToDisplayFrames =
      rLog.RegisterValue("pacerDisplayStartToDisplayFrames", TraceUnit::Count,
                         "The frames whose time from their start to their display the pacer counted, since the pacer was made");
    rColumns.DisplayStartToDisplayTotal =
      rLog.RegisterValue("pacerDisplayStartToDisplayTotalNs", TraceUnit::Nanoseconds, "The sum of those times from a frame's start to its display");
    rColumns.DisplayStartToDisplayLongest =
      rLog.RegisterValue("pacerDisplayStartToDisplayLongestNs", TraceUnit::Nanoseconds, "The longest of those times");
    rColumns.DisplayLateFrames = rLog.RegisterValue("pacerDisplayLateFrames", TraceUnit::Count,
                                                    "Of those, the frames shown later: the frame before them was on screen a refresh or "
                                                    "more longer than it was made for");
    rColumns.PresentTarget = rLog.RegisterValue("presentTargetTicks", TraceUnit::DurationTicks,
                                                "The target time the sample asked for: the frame is not to be shown before this long after "
                                                "the frame before it was shown");
    rColumns.CpuSpike = rLog.RegisterValue("cpuSpikeMs", TraceUnit::Count,
                                           "The milliseconds the frame was busy on top of the CPU load: a frame that runs long now and "
                                           "then (--CpuSpike), zero in the frames between");
    rColumns.PacerKind = rLog.RegisterValue("pacerKind", TraceUnit::Code,
                                            "What the pacer of the library paced the frame with: 1 a timer and the refresh period, 2 a "
                                            "timer with a wait for a present, 3 the vertical blank times and the refresh period, 4 the "
                                            "vertical blank times with a wait for a present. The pacer gives the times the sample "
                                            "waits until");
    rColumns.RefreshesBehindClock = rLog.RegisterValue("pacerRefreshesBehindClock", TraceUnit::Count,
                                                       "The refreshes that were lost and that the animation time was not moved over, counted since "
                                                       "the pacer was made, as it was when the frame started");
    rColumns.GpuWaitTimeouts = rLog.RegisterValue("pacerGpuWaitTimeouts", TraceUnit::Count,
                                                  "With --Pacer.GpuWait: the waits for the GPU's work on a frame that ended without the GPU done "
                                                  "with it, counted since the pacer was made");
    rColumns.PresentWaitTimeouts =
      rLog.RegisterValue("pacerPresentWaitTimeouts", TraceUnit::Count,
                         "pacerKind 2 and 4: the waits for a present that ended without the present being shown, counted since the pacer was "
                         "made");
    rColumns.PresentWaitsStopped =
      rLog.RegisterValue("pacerPresentWaitsStopped", TraceUnit::Flag,
                         "pacerKind 2 and 4: 1 while the pacer does not wait for presents because its waits ran out (a window that is not shown): it "
                         "paces on its timer and only asks if a present was shown");
    rColumns.PacerGpuTime = rLog.RegisterValue("pacerGpuTimeTicks", TraceUnit::DurationTicks,
                                               "The GPU time the pacer judges a frame with, as it was when the "
                                               "frame started: the newest it was given (the sample gives it the GPU work of every "
                                               "frame it has measured, with the times where it has them)");
    rColumns.StartupPauses = rLog.RegisterValue("pacerStartupPauses", TraceUnit::Count,
                                                "pacerKind 1 and 3: the pauses the pacer made after a start (half a second after its "
                                                "first frame, and again after a new swapchain), counted since the pacer was made");
    rColumns.VBlankReading = rLog.RegisterValue("pacerVBlankReading", TraceUnit::Flag,
                                                "pacerKind 3 and 4: 1 once the pacer was given a vertical blank of the display it is on. Before "
                                                "that it paces on its clock");
    rColumns.VBlankJumps = rLog.RegisterValue("pacerVBlankJumps", TraceUnit::Count,
                                              "pacerKind 3 and 4: the vertical blanks the pacer was given that were more than a eighth of a "
                                              "refresh off where the ones before put them, counted since the pacer was made");
    rColumns.ShownLaterByWaits = rLog.RegisterValue("pacerShownLaterByWaits", TraceUnit::Count,
                                                    "pacerKind 4: the refreshes a wait for a present said a frame was shown later than the "
                                                    "pacer had worked out, counted since the pacer was made");
    rColumns.ReadyPlace = rLog.RegisterValue("pacerReadyPlaceTicks", TraceUnit::DurationTicks,
                                             "pacerKind 4: where in a refresh the pacer has a frame ready now, as the time after the "
                                             "vertical blank before it. It starts at --Pacer.ReadyPlace and the pacer moves it earlier "
                                             "when frames that were ready there are shown a vertical blank late");
    rColumns.ReadyPlaceTries = rLog.RegisterValue("pacerReadyPlaceTries", TraceUnit::Count,
                                                  "pacerKind 4: the times the pacer tried that place one step later again, counted since "
                                                  "the pacer was made");
    rColumns.ReadyPlaceTriesTakenBack =
      rLog.RegisterValue("pacerReadyPlaceTriesTakenBack", TraceUnit::Count,
                         "pacerKind 4: the tries it took back, because a frame was shown later in the frames after one");
    rColumns.SystemHeldFrames = rLog.RegisterValue("pacerSystemHeldFrames", TraceUnit::Count,
                                                   "pacerKind 1 and 2: the frames whose start the side of the display held (a acquire, a present "
                                                   "that waited: for a eighth of a refresh or more), counted since the pacer was made");
    rColumns.FrameSlotHeldFrames = rLog.RegisterValue("pacerFrameSlotHeldFrames", TraceUnit::Count,
                                                      "pacerKind 1 and 2: the frames whose start a wait for a frame slot held for a eighth of a "
                                                      "refresh or more (the GPU was not done with a earlier frame), counted since the pacer "
                                                      "was made");
    rColumns.DisplayHeldRefreshes =
      rLog.RegisterValue("pacerDisplayHeldRefreshes", TraceUnit::Count,
                         "pacerKind 3 and 4: the refreshes frame starts were late by while the side of the display held the loop (a "
                         "acquire, a present that waited, a wait for a frame slot or for the GPU's work while the GPU did no work), which "
                         "the animation time does not step over, counted since the pacer was made");
    rColumns.AnimationError = rLog.RegisterValue(
      "animationErrorTicks", TraceUnit::DurationTicks,
      "The animation error of the frame by the display times the system reports: how far the animation moved from the frame presented "
      "before it, less how long after that frame it was shown. Negative: shown too late. Empty unless both frames have a display time");
    rLog.SetFact("sample.presentMethod", m_presentMethod == SamplePresentMethod::WaitThenPresent ? "WaitThenPresent" : "SwapInterval");
    rLog.SetFact("sample.pacerKind", ToString(m_pacerKind));
    rLog.SetFact("sample.pacerKindChangeFrames", fmt::format("{}", m_kindChangeFrames));
    rLog.SetFact("sample.pacerSupported", SamplePacer::IsSupported() ? "1" : "0");
    // What a frame started with is in the row of that frame. A log without this fact has it in the row of the frame before when the
    // frame starts in the update (SamplePresentMethod::SwapInterval).
    rLog.SetFact("sample.frameStartRow", "own");
    rLog.SetFact("sample.glFlush", m_flushWanted ? "1" : "0");
  }


  void FramePacingShared::LogFrameStart(const TickCount waitStartTime)
  {
    if (!m_trace)
    {
      return;
    }
    ITraceService& rLog = *m_trace;
    const LogColumns& columns = m_logColumns;
    const bool pacerOn = m_pacer != nullptr;
    if (!m_hasLoggedPacerConfig || pacerOn != m_loggedPacerOn || m_pacerConfig != m_loggedPacerConfig)
    {
      // The settings the frames from here on are paced with
      m_hasLoggedPacerConfig = true;
      m_loggedPacerOn = pacerOn;
      m_loggedPacerConfig = m_pacerConfig;
      rLog.AddEvent("pacerConfig",
                    fmt::format("on={};refreshRateHz={};targetFps={};adaptive={};presentFeedback={};kind={};waitingPresents={};"
                                "framesInFlight={};startupPauseRefreshes={};aim={};refreshPeriodNs={};readyPlacePercent={};swapChainImages={};"
                                "systemHoldsLoop={};timedPresent={};gpuWait={};systemWaits={}",
                                pacerOn ? 1 : 0, NanosecondTimeSpanUtil::ToFrequencyHz(m_pacerConfig.RefreshPeriod), m_pacerConfig.TargetFps,
                                m_pacerConfig.Adaptive ? 1 : 0, m_pacerConfig.PresentFeedback ? 1 : 0, ToString(m_pacerConfig.Kind),
                                m_pacerConfig.WaitingPresents, m_pacerConfig.MaxFramesInFlight,
                                m_pacerConfig.Kind == SamplePacerKind::TimerPeriodOnly || m_pacerConfig.Kind == SamplePacerKind::VBlankPeriodOnly
                                  ? m_pacerConfig.StartupPauseRefreshes
                                  : 0u,
                                ToString(m_pacerConfig.Aim), m_pacerConfig.RefreshPeriod.TotalNanoseconds(), m_pacerConfig.ReadyPlacePercent,
                                m_pacerConfig.SwapChainImages, m_pacerConfig.SystemHoldsLoop ? 1 : 0, m_pacerConfig.TimedPresent ? 1 : 0,
                                m_pacerConfig.GpuWait ? 1 : 0, m_pacerSystemWaits ? 1 : 0));
    }

    {
      // What makes the GPU load: the same --GpuLoad is another amount of work with another scene, so a log that does not say
      // which one it was can not be compared with another
      const RaymarchScene scene = GetBackgroundScene();
      if (!m_hasLoggedBackground || scene != m_loggedBackgroundScene)
      {
        m_hasLoggedBackground = true;
        m_loggedBackgroundScene = scene;
        rLog.AddEvent("background", fmt::format("scene={}", ToLogName(scene)));
      }
    }

    if (m_variableRefreshSeen != m_loggedVariableRefreshSeen)
    {
      // From here on the pacer is not given the vertical blank times (or is given them again): they follow the frames on a display
      // that refreshes at a variable rate
      m_loggedVariableRefreshSeen = m_variableRefreshSeen;
      rLog.AddEvent("holdVariableRefresh", fmt::format("seen={}", m_variableRefreshSeen ? 1 : 0));
    }

    rLog.SetValue(columns.PacerOn, pacerOn);
    rLog.SetValue(columns.FrameWaitStart, waitStartTime);
    if (m_frameStartLogTargetTime.Ticks() != 0)
    {
      rLog.SetValue(columns.FrameWaitTarget, m_frameStartLogTargetTime);
    }
    rLog.SetValue(columns.FrameStart, m_frameStartTime);
    rLog.SetInt64(columns.CpuLoad, m_ui.SliderCpuLoad->GetValue());
    rLog.SetInt64(columns.CpuSpike, m_frameCpuSpikeMs);
    rLog.SetInt64(columns.GpuLoad, m_ui.SliderGpuLoad->GetValue());
    if (pacerOn)
    {
      rLog.SetUInt64(columns.SwapInterval, m_schedule.SwapInterval);
      rLog.SetInt64(columns.Change, static_cast<int64_t>(m_schedule.Change));
      rLog.SetValue(columns.AnimationStep, m_schedule.AnimationStep);
      // The frame window as it is after the frame before this one was measured
      const SamplePacerStatus status = m_pacer->GetStatus();
      rLog.SetUInt64(columns.PreferredSwapInterval, status.PreferredSwapInterval);
      rLog.SetUInt64(columns.WindowFrames, status.Frames);
      rLog.SetUInt64(columns.WindowLateFrames, status.LateFrames);
      rLog.SetValue(columns.WindowStartsAhead, status.StartsAhead);
      rLog.SetValue(columns.WindowAverageWork, status.AverageWork);
      rLog.SetValue(columns.WindowSpan, status.WindowSpan);
      rLog.SetValue(columns.WindowFull, status.WindowFull);
      rLog.SetUInt64(columns.PacerFrameId, m_schedule.FrameId);
      rLog.SetValue(columns.NextFrameStart, m_schedule.NextFrameStartTime);
      rLog.SetInt64(columns.PacerKind, ToLogCode(m_pacerConfig.Kind));
      rLog.SetUInt64(columns.RefreshesBehindClock, m_pacer->GetRefreshesBehindClock());
      rLog.SetValue(columns.PacerGpuTime, m_pacer->GetGpuTime());
      if (m_pacerConfig.Kind == SamplePacerKind::TimerPeriodOnly || m_pacerConfig.Kind == SamplePacerKind::VBlankPeriodOnly)
      {
        rLog.SetUInt64(columns.StartupPauses, m_pacer->GetStartupPauses());
      }
      if (m_pacerConfig.Kind == SamplePacerKind::TimerPeriodOnly || m_pacerConfig.Kind == SamplePacerKind::TimerWaitForPresent)
      {
        // The pacer takes the waits of the app where it paces on its clock
        rLog.SetUInt64(columns.SystemHeldFrames, m_pacer->GetSystemHeldFrames());
        rLog.SetUInt64(columns.FrameSlotHeldFrames, m_pacer->GetFrameSlotHeldFrames());
      }
      if (IsVBlankKind(m_pacerConfig.Kind))
      {
        rLog.SetUInt64(columns.DisplayHeldRefreshes, m_pacer->GetDisplayHeldRefreshes());
        rLog.SetInt64(columns.VBlankReading, m_pacer->HasVBlankReading() ? 1 : 0);
        rLog.SetUInt64(columns.VBlankJumps, m_pacer->GetVBlankJumps());
      }
      if (m_pacerConfig.Kind == SamplePacerKind::VBlankWaitForPresent)
      {
        rLog.SetUInt64(columns.ShownLaterByWaits, m_pacer->GetShownLaterByWaits());
        rLog.SetValue(columns.ReadyPlace, m_pacer->GetReadyPlaceNow());
        rLog.SetUInt64(columns.ReadyPlaceTries, m_pacer->GetReadyPlaceTries());
        rLog.SetUInt64(columns.ReadyPlaceTriesTakenBack, m_pacer->GetReadyPlaceTriesTakenBack());
      }
      if (m_pacerConfig.GpuWait)
      {
        rLog.SetUInt64(columns.GpuWaitTimeouts, m_pacer->GetGpuWaitTimeouts());
      }
      if (IsPresentWaitKind(m_pacerConfig.Kind))
      {
        rLog.SetUInt64(columns.PresentWaitTimeouts, m_pacer->GetPresentWaitTimeouts());
        rLog.SetInt64(columns.PresentWaitsStopped, m_pacer->IsPresentWaitStopped() ? 1 : 0);
      }
      rLog.SetValue(columns.FeedbackOn, m_pacerConfig.PresentFeedback);
      if (m_pacerConfig.PresentFeedback)
      {
        // What the display reports the pacer had when it planned this frame say of the frames before it
        const SamplePacerDisplayErrors displayErrors = m_pacer->GetDisplayErrors();
        rLog.SetUInt64(columns.DisplayReports, displayErrors.Reports);
        rLog.SetUInt64(columns.DisplayRefused, displayErrors.Refused);
        rLog.SetUInt64(columns.DisplayJudgedFrames, displayErrors.JudgedFrames);
        rLog.SetUInt64(columns.DisplayErrorFrames, displayErrors.ErrorFrames);
        rLog.SetUInt64(columns.DisplayOffTargetFrames, displayErrors.OffTargetFrames);
        rLog.SetUInt64(columns.DisplayLateFrames, displayErrors.LateFrames);
        rLog.SetUInt64(columns.DisplayStartToDisplayFrames, displayErrors.StartToDisplayFrames);
        rLog.SetValue(columns.DisplayStartToDisplayTotal, displayErrors.StartToDisplayTotal);
        rLog.SetValue(columns.DisplayStartToDisplayLongest, displayErrors.StartToDisplayLongest);
      }
    }
  }


  NanosecondTimeSpan FramePacingShared::ReadDisplayRefreshPeriod() const
  {
    const auto window = m_window.lock();
    // This is cheap as the window caches it
    return window ? window->TryGetDisplayInfo().RefreshInterval : NanosecondTimeSpan();
  }


  bool FramePacingShared::UpdateDetectedRefreshPeriod()
  {
    const NanosecondTimeSpan detectedRefreshPeriod = ReadDisplayRefreshPeriod();
    if (detectedRefreshPeriod == m_detectedRefreshPeriod)
    {
      return false;
    }
    m_detectedRefreshPeriod = detectedRefreshPeriod;
    m_detectedRefreshRateHz = NanosecondTimeSpanUtil::ToFrequencyHz(detectedRefreshPeriod);
    return true;
  }


  NanosecondTimeSpan FramePacingShared::GetRefreshPeriod() const
  {
    if (!m_refreshRateOverrideHz.has_value() && m_detectedRefreshPeriod.TotalNanoseconds() > 0)
    {
      // The period of the window system as it is: it does not go through a rate
      return m_detectedRefreshPeriod;
    }
    const double refreshRateHz = GetRefreshRateHz();
    return refreshRateHz > 0.0 ? NanosecondTimeSpan(std::llround(static_cast<double>(NanosecondTimeSpan::NanosecondsPerSecond) / refreshRateHz))
                               : NanosecondTimeSpan();
  }


  double FramePacingShared::GetRefreshRateHz() const
  {
    if (m_refreshRateOverrideHz.has_value())
    {
      return m_refreshRateOverrideHz.value();
    }
    return m_detectedRefreshRateHz > 0.0 ? m_detectedRefreshRateHz : static_cast<double>(m_ui.SliderRefreshRate->GetValue());
  }


  void FramePacingShared::UpdateRefreshRateUI()
  {
    // The slider is only used when nothing else knows the refresh rate, otherwise it shows the rate that is used
    const bool isKnown = m_refreshRateOverrideHz.has_value() || m_detectedRefreshRateHz > 0.0;
    m_ui.SliderRefreshRate->SetEnabled(!isKnown);
    if (!isKnown)
    {
      m_ui.LabelRefreshRate->SetContent("Refresh rate: unknown, set it (Hz)");
      return;
    }
    // The rate that is used and where it is from are in the frame pacing overlay
    const double refreshRateHz = GetRefreshRateHz();
    m_ui.SliderRefreshRate->SetValue(static_cast<int32_t>(std::lround(refreshRateHz)));
    m_ui.LabelRefreshRate->SetContent("Refresh rate (Hz)");
  }


  void FramePacingShared::UpdateTier()
  {
    if (!m_tierFactsReady)
    {
      return;
    }
    // The tiers are the pacer library's, and so is the rule that gives a tier: the sample only says what the app has and what the
    // run uses, as capabilities, and the library rates them.
    TierState state;
    // The best here: everything this app can do on this system right now
    state.Best = SamplePacer::Rate(GetPacerCapabilities());
    if (m_pacer)
    {
      // The pacer says which tier paces the frame now
      state.InUse = m_pacer->GetTier();
    }

    // The radio buttons say the pacer the run is paced by, and the one it will be paced by while the frame pacer is off
    ShowTiers(m_pacer ? m_pacerConfig.Kind : m_selectedKind);

    if (m_tierKnown && state == m_tierState)
    {
      return;
    }
    // From here on: only when it changed (the pacer was switched, another pacer, the system lost or got a vsync time)
    m_tierKnown = true;
    m_tierState = state;

    // The tier as the pacer library writes it: its major tier and its sub tier ("3.1")
    const std::string_view best = SamplePacer::GetTierNumber(state.Best.Tier, state.Best.TimedPresent);
    const std::string_view bestName = SamplePacer::GetTierLogName(state.Best.Tier, state.Best.TimedPresent);
    // "0" and "pacerOff": the pacer is off, so the run is in no tier. The pacer in use is one where the loop places the frame: the
    // sample has no present at a time.
    const std::string_view tier = state.InUse.has_value() ? SamplePacer::GetTierNumber(*state.InUse) : std::string_view("0");
    const std::string_view tierName = state.InUse.has_value() ? SamplePacer::GetTierLogName(*state.InUse) : std::string_view("pacerOff");
    const int32_t displaySideHolds = state.Best.DisplaySideHolds ? 1 : 0;
    if (m_trace)
    {
      // A event and not a fact: the tier of a run can change, and its first value can be from before the window system had a
      // vsync time
      m_trace->AddEvent("tier",
                        fmt::format("tier={};tierName={};best={};bestName={};displaySideHolds={}", tier, tierName, best, bestName, displaySideHolds));
      FSLLOG3_INFO("FramePacing: tier in use: {} ({}); best here: {} ({}); the display side holds: {}; vsync source '{}'", tier, tierName, best,
                   bestName, displaySideHolds, m_vsyncSourceName);
    }
    else
    {
      FSLLOG3_VERBOSE("FramePacing: tier in use: {} ({}); best here: {} ({}); the display side holds: {}; vsync source '{}'", tier, tierName, best,
                      bestName, displaySideHolds, m_vsyncSourceName);
    }
  }


  TimeSpan FramePacingShared::GetTargetFrameTime() const noexcept
  {
    if (m_pacer && m_schedule.TargetFrameTime.TotalNanoseconds() > 0)
    {
      return NanosecondTimeSpanUtil::ToTimeSpan(m_schedule.TargetFrameTime);
    }
    // Without the frame pacer every frame is held for one refresh
    return NanosecondTimeSpanUtil::ToTimeSpan(m_pacerConfig.RefreshPeriod);
  }


  void FramePacingShared::UpdatePacerStats()
  {
    if (!m_ui.SwitchPacerStats->IsChecked())
    {
      // The overlay is hidden
      return;
    }
    m_pacerStatsLevels = {};
    FillPacerStats();

    // A value that asks to be looked at is yellow, one that says the run does not do what it aims for is red
    const PacerStatsUIRecord& rStats = m_ui.PacerStats;
    const UI::UIColor normalColor = m_ui.DefaultFontColor;
    rStats.SwapInterval->SetFontColor(ToColor(m_pacerStatsLevels.SwapInterval, normalColor));
    rStats.PacedRate->SetFontColor(ToColor(m_pacerStatsLevels.SwapInterval, normalColor));
    rStats.FrameTime->SetFontColor(ToColor(m_pacerStatsLevels.FrameTime, normalColor));
    rStats.LateFrames->SetFontColor(ToColor(m_pacerStatsLevels.LateFrames, normalColor));
    rStats.AverageWork->SetFontColor(ToColor(m_pacerStatsLevels.AverageWork, normalColor));
    rStats.FeedbackLate->SetFontColor(ToColor(m_pacerStatsLevels.FeedbackLate, normalColor));
    rStats.VariableRefresh->SetFontColor(ToColor(m_pacerStatsLevels.VariableRefresh, normalColor));
    rStats.DisplayInterval->SetFontColor(ToColor(m_pacerStatsLevels.DisplayInterval, normalColor));
    rStats.DisplayRefresh->SetFontColor(ToColor(m_pacerStatsLevels.DisplayRefresh, normalColor));
  }


  void FramePacingShared::FillPacerStats()
  {
    const PacerStatsUIRecord& rStats = m_ui.PacerStats;
    const TimeSpan targetFrameTime = GetTargetFrameTime();

    // What the sample measures, with the frame pacer on or off
    const SampleFrameTimes frameTimes = m_frameStats.FrameTimes();
    if (m_frameStats.FrameCount() > 0)
    {
      SetFormattedContent(*rStats.FrameTime, "{:.2f} ms ({:.2f} to {:.2f})", frameTimes.Average.TotalMilliseconds(),
                          frameTimes.Min.TotalMilliseconds(), frameTimes.Max.TotalMilliseconds());
      m_pacerStatsLevels.FrameTime = SampleStatsLevelUtil::RateFrameTime(frameTimes.Average, frameTimes.Max, targetFrameTime);
    }
    else
    {
      rStats.FrameTime->SetContent(UnknownValue);
    }
    if (m_lastGpuTime.Ticks() > 0)
    {
      SetFormattedContent(*rStats.Work, "CPU {:.2f} ms, GPU {:.2f} ms", m_lastCpuTime.TotalMilliseconds(), m_lastGpuTime.TotalMilliseconds());
    }
    else
    {
      SetFormattedContent(*rStats.Work, "CPU {:.2f} ms", m_lastCpuTime.TotalMilliseconds());
    }
    if (m_lastPresentWait.Ticks() > 0)
    {
      SetFormattedContent(*rStats.PresentWait, "{:.2f} ms", m_lastPresentWait.TotalMilliseconds());
    }
    else
    {
      rStats.PresentWait->SetContent("none");
    }
    UpdatePresentFeedbackStats();
    UpdateVariableRefreshStats();

    // The refresh rate the pacing counts with, and where it is from
    const double refreshRateHz = GetRefreshRateHz();
    {
      const bool isKnown = m_refreshRateOverrideHz.has_value() || m_detectedRefreshRateHz > 0.0;
      SetFormattedContent(*rStats.RefreshRate, "{:.2f} Hz ({})", refreshRateHz,
                          m_refreshRateOverrideHz.has_value() ? "command line" : (isKnown ? "display" : "set with the slider"));
    }

    // If the present of the app can hold a frame for two refreshes or more, by the rating of the pacer library
    if (!SamplePacer::IsSupported())
    {
      rStats.DisplaySideHolds->SetContent("pacer not supported");
    }
    else if (!m_tierKnown)
    {
      rStats.DisplaySideHolds->SetContent("unknown");
    }
    else
    {
      rStats.DisplaySideHolds->SetContent(m_tierState.Best.DisplaySideHolds ? "yes" : "no");
    }

    if (!m_pacer)
    {
      // Without the pacer every frame is held for one refresh and the sample counts the late frames
      const uint32_t frames = m_frameStats.FrameCount();
      const uint32_t lateFrames = m_frameStats.LateFrameCount();
      SetFormattedContent(*rStats.PacedRate, "{:.2f} Hz ({})", refreshRateHz, SamplePacer::IsSupported() ? PacerOffValue : "pacer not supported");
      SetFormattedContent(*rStats.SwapInterval, "1 ({})", SamplePacer::IsSupported() ? PacerOffValue : "pacer not supported");
      SetFormattedContent(*rStats.LateFrames, "{} of {} ({:.1f} %)", lateFrames, frames, frames > 0u ? ((100.0 * lateFrames) / frames) : 0.0);
      m_pacerStatsLevels.LateFrames = SampleStatsLevelUtil::RateLateFrames(lateFrames, frames);
      for (UI::Label* pLabel : {rStats.AverageWork.get(), rStats.IntervalChanges.get(), rStats.LastChange.get(), rStats.FrameWindow.get(),
                                rStats.Feedback.get(), rStats.FeedbackLate.get()})
      {
        pLabel->SetContent(PacerOffValue);
      }
      return;
    }

    // What the frame pacer decides on
    const SamplePacerStatus status = m_pacer->GetStatus();
    // The rate the frames are paced at, and the one the app prefers (the target frame rate as the display can show it)
    SetFormattedContent(*rStats.PacedRate, "{:.2f} Hz (preferred {:.2f})", refreshRateHz / static_cast<double>(std::max(status.SwapInterval, 1u)),
                        refreshRateHz / static_cast<double>(std::max(status.PreferredSwapInterval, 1u)));
    SetFormattedContent(*rStats.SwapInterval, "{} (preferred {})", status.SwapInterval, status.PreferredSwapInterval);
    // The frames are held for longer than the target frame rate asks for
    m_pacerStatsLevels.SwapInterval = status.SwapInterval > status.PreferredSwapInterval ? SampleStatsLevel::Warning : SampleStatsLevel::Normal;
    SetFormattedContent(*rStats.LateFrames, "{} of {} ({:.1f} %)", status.LateFrames, status.Frames,
                        status.Frames > 0u ? ((100.0 * status.LateFrames) / status.Frames) : 0.0);
    m_pacerStatsLevels.LateFrames = SampleStatsLevelUtil::RateLateFrames(status.LateFrames, status.Frames);
    if (status.Frames > 0u && m_schedule.TargetFrameTime.TotalNanoseconds() > 0)
    {
      SetFormattedContent(*rStats.AverageWork, "{:.2f} ms, {:.0f} % of the frame time", status.AverageWork.TotalMilliseconds(),
                          (100.0 * status.AverageWork.TotalMilliseconds()) / m_schedule.TargetFrameTime.TotalMilliseconds());
      m_pacerStatsLevels.AverageWork =
        SampleStatsLevelUtil::RateWork(status.AverageWork, NanosecondTimeSpanUtil::ToTimeSpan(m_schedule.TargetFrameTime));
    }
    else
    {
      rStats.AverageWork->SetContent(UnknownValue);
    }
    // How often the frame pacer made the swap interval longer and shorter, and for how long that has been counted: since the frame
    // pacer was switched on or one of its settings was changed
    SetFormattedContent(*rStats.IntervalChanges, "{} slower, {} faster (counted for {:.0f} s)", m_pacerChanges.SlowerCount,
                        m_pacerChanges.FasterCount, (m_timer.GetTimestamp() - m_pacerChanges.StartTime).TotalSeconds());
    if (m_pacerChanges.LastChange != SamplePacerChange::Unchanged)
    {
      SetFormattedContent(*rStats.LastChange, "{}, {:.1f} s ago", m_pacerChanges.LastChange == SamplePacerChange::Slower ? "slower" : "faster",
                          (m_timer.GetTimestamp() - m_pacerChanges.LastChangeTime).TotalSeconds());
    }
    else
    {
      rStats.LastChange->SetContent("none");
    }
    SetFormattedContent(*rStats.FrameWindow, "{:.2f} s{}", status.WindowSpan.TotalSeconds(), status.WindowFull ? ", full" : "");
    if (m_pacerConfig.PresentFeedback)
    {
      // The pacer counts the animation error from the display times
      const SamplePacerDisplayErrors displayErrors = m_pacer->GetDisplayErrors();
      SetFormattedContent(*rStats.Feedback, "{} judged, {} refused", displayErrors.JudgedFrames, displayErrors.Refused);
      SetFormattedContent(*rStats.FeedbackLate, "{} frames", displayErrors.LateFrames);
      m_pacerStatsLevels.FeedbackLate = displayErrors.LateFrames > 0u ? SampleStatsLevel::Warning : SampleStatsLevel::Normal;
    }
    else
    {
      rStats.Feedback->SetContent(SwitchedOffValue);
      rStats.FeedbackLate->SetContent(SwitchedOffValue);
    }
  }


  void FramePacingShared::UpdateVariableRefreshStats()
  {
    // What the window knows, what was measured and what the swapchain says are three sources, so the row names the one it has
    const auto window = m_window.lock();
    const NativeWindowVariableRefreshInfo info = window ? window->TryGetVariableRefreshInfo() : NativeWindowVariableRefreshInfo();
    const char* pszSwapchain = "";
    switch (m_swapchainRefresh)
    {
    case SampleSwapchainRefresh::Fixed:
      pszSwapchain = ", swapchain: fixed";
      break;
    case SampleSwapchainRefresh::Variable:
      pszSwapchain = ", swapchain: variable";
      break;
    case SampleSwapchainRefresh::Unknown:
    default:
      break;
    }
    UI::Label& rLabel = *m_ui.PacerStats.VariableRefresh;
    // With a variable refresh rate the display follows the frames, so nothing here holds a frame for a refresh
    m_pacerStatsLevels.VariableRefresh =
      (info.Active == NativeWindowVariableRefreshAnswer::Yes || info.Observed == NativeWindowVariableRefreshAnswer::Yes || m_variableRefreshSeen)
        ? SampleStatsLevel::Warning
        : SampleStatsLevel::Normal;
    if (info.Active == NativeWindowVariableRefreshAnswer::Yes)
    {
      SetFormattedContent(rLabel, "active ({}){}", info.Source, pszSwapchain);
    }
    else if (info.Observed == NativeWindowVariableRefreshAnswer::Yes)
    {
      SetFormattedContent(rLabel, "seen, {:.1f} refreshes per frame{}", static_cast<double>(info.ObservedIntervalMilliPeriods) / 1000.0,
                          pszSwapchain);
    }
    else if (m_variableRefreshSeen)
    {
      SetFormattedContent(rLabel, "seen before{}", pszSwapchain);
    }
    else if (info.Observed == NativeWindowVariableRefreshAnswer::No)
    {
      SetFormattedContent(rLabel, "not seen{}", pszSwapchain);
    }
    else
    {
      SetFormattedContent(rLabel, "{}{}", UnknownValue, pszSwapchain);
    }
  }


  void FramePacingShared::UpdatePresentFeedbackStats()
  {
    const PacerStatsUIRecord& rStats = m_ui.PacerStats;

    // The GPU work can be placed on the timeline without measured presents
    if (!IsGpuTimelineWanted())
    {
      m_presentFeedback.ClearGpuInterval();
      rStats.GpuWork->SetContent(m_gpuTimelineSupported ? SwitchedOffValue : NotSupportedValue);
    }
    else
    {
      const std::optional<TimeSpan> gpuStart = m_presentFeedback.GetLastGpuStart();
      const std::optional<TimeSpan> gpuEnd = m_presentFeedback.GetLastGpuEnd();
      if (gpuStart.has_value() && gpuEnd.has_value())
      {
        // When the GPU started and finished the frame, counted from when the CPU started on it
        SetFormattedContent(*rStats.GpuWork, "{:+.2f} to {:+.2f} ms", gpuStart.value().TotalMilliseconds(), gpuEnd.value().TotalMilliseconds());
      }
      else
      {
        rStats.GpuWork->SetContent(NotMeasuredValue);
      }
    }

    if (!m_presentFeedbackEnabled)
    {
      // Supported and switched on but not measured: the surface does not support it
      const char* const pszReason =
        !m_presentTimingSupported ? NotSupportedValue : (m_ui.SwitchPresentTiming->IsChecked() ? NotMeasuredValue : SwitchedOffValue);
      for (UI::Label* pLabel : {rStats.DisplayError.get(), rStats.DisplayInterval.get(), rStats.AnimationError.get(), rStats.Latency.get(),
                                rStats.TimedFrames.get(), rStats.DisplayRefresh.get()})
      {
        pLabel->SetContent(pszReason);
      }
      return;
    }

    const SamplePresentFeedbackStats stats = m_presentFeedback.CalcStats();
    if (stats.AverageDisplayError.has_value() && stats.WorstDisplayError.has_value())
    {
      // How far the frames were shown from the time the frame pacer aimed for
      SetFormattedContent(*rStats.DisplayError, "{:+.2f} ms (worst {:+.2f})", stats.AverageDisplayError.value().TotalMilliseconds(),
                          stats.WorstDisplayError.value().TotalMilliseconds());
    }
    else
    {
      rStats.DisplayError->SetContent(m_pacer ? UnknownValue : PacerOffValue);
    }
    if (stats.AverageDisplayInterval.has_value() && stats.MinDisplayInterval.has_value() && stats.MaxDisplayInterval.has_value())
    {
      // How evenly the frames reach the display, which is what the frame pacer is for
      SetFormattedContent(*rStats.DisplayInterval, "{:.2f} ms ({:.2f} to {:.2f})", stats.AverageDisplayInterval.value().TotalMilliseconds(),
                          stats.MinDisplayInterval.value().TotalMilliseconds(), stats.MaxDisplayInterval.value().TotalMilliseconds());
      m_pacerStatsLevels.DisplayInterval =
        SampleStatsLevelUtil::RateFrameTime(stats.AverageDisplayInterval.value(), stats.MaxDisplayInterval.value(), GetTargetFrameTime());
    }
    else
    {
      rStats.DisplayInterval->SetContent(UnknownValue);
    }
    {
      // How far the animation was off where the frames were shown, which is what is seen as stutter
      const SampleAnimationErrorStats animationErrorStats = m_animationError.CalcStats();
      if (animationErrorStats.Frames > 0u)
      {
        SetFormattedContent(*rStats.AnimationError, "{} of {} over {:.0f} ms (worst {:+.2f})", animationErrorStats.ErrorFrames,
                            animationErrorStats.Frames, SampleAnimationError::ErrorThreshold.TotalMilliseconds(),
                            animationErrorStats.WorstError.TotalMilliseconds());
      }
      else
      {
        rStats.AnimationError->SetContent(UnknownValue);
      }
    }
    if (stats.AverageLatency.has_value() && stats.AverageQueueTime.has_value())
    {
      SetFormattedContent(*rStats.Latency, "{:.2f} ms (handed over after {:.2f})", stats.AverageLatency.value().TotalMilliseconds(),
                          stats.AverageQueueTime.value().TotalMilliseconds());
    }
    else if (stats.AverageLatency.has_value())
    {
      SetFormattedContent(*rStats.Latency, "{:.2f} ms", stats.AverageLatency.value().TotalMilliseconds());
    }
    else
    {
      rStats.Latency->SetContent(UnknownValue);
    }
    SetFormattedContent(*rStats.TimedFrames, "{} of {}", stats.TimedFrames, stats.MeasuredFrames);
    if (m_measuredRefreshDuration.Ticks() > 0)
    {
      // What the swapchain reports is not always the refresh of the display the window is on: with displays at different rates it was
      // seen to be the one of the fastest display. So the row says when the two differ.
      const double swapchainHz = static_cast<double>(TimeSpan::TicksPerSecond) / static_cast<double>(m_measuredRefreshDuration.Ticks());
      const bool isDisplayRate = m_detectedRefreshRateHz <= 0.0 ||
                                 std::abs(swapchainHz - m_detectedRefreshRateHz) <= (m_detectedRefreshRateHz * LocalConfig::SameRefreshRateTolerance);
      if (isDisplayRate)
      {
        SetFormattedContent(*rStats.DisplayRefresh, "{:.3f} ms ({:.2f} Hz)", m_measuredRefreshDuration.TotalMilliseconds(), swapchainHz);
      }
      else
      {
        SetFormattedContent(*rStats.DisplayRefresh, "{:.3f} ms ({:.2f} Hz), display {:.2f} Hz", m_measuredRefreshDuration.TotalMilliseconds(),
                            swapchainHz, m_detectedRefreshRateHz);
        m_pacerStatsLevels.DisplayRefresh = SampleStatsLevel::Warning;
      }
    }
    else
    {
      rStats.DisplayRefresh->SetContent(UnknownValue);
    }
  }


  void FramePacingShared::BurnCpu(const TimeSpan duration) const
  {
    if (duration.Ticks() <= 0)
    {
      return;
    }
    const ScopedTraceZone traceZone(m_trace.get(), m_traceZones.CpuLoad);
    // Busy on purpose: a sleeping thread would not be a CPU load
    const TickCount endTime = m_timer.GetTimestamp() + duration;
    while (m_timer.GetTimestamp() < endTime)
    {
    }
  }


  std::shared_ptr<UI::BaseWindow> FramePacingShared::CreateStatsWindow(UI::Theme::IThemeControlFactory& rUIFactory)
  {
    // Two overlays: every value the last drawn marker carried and the frame pacing stats (top right, next to the right bar and away from
    // the markers on the left side). Each is a grid of names and values, the columns have the same widths so the overlays line up.
    const auto createGrid = [&rUIFactory]()
    {
      auto grid = std::make_shared<UI::GridLayout>(rUIFactory.GetContext());
      grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Fixed, StatsNameColumnWidthDp));
      grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Fixed, StatsValueColumnWidthDp));
      return grid;
    };
    const auto addStatsRow = [&rUIFactory](UI::GridLayout& rGrid, uint32_t& rRow, const char* const pszName)
    {
      rGrid.AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Auto));
      const auto nameLabel = rUIFactory.CreateLabel(pszName);
      auto valueLabel = rUIFactory.CreateLabel(UnknownValue);
      rGrid.AddChild(nameLabel, 0, rRow);
      rGrid.AddChild(valueLabel, 1, rRow);
      ++rRow;
      return valueLabel;
    };

    const auto markerGrid = createGrid();
    uint32_t markerRow = 0;
    MarkerStatsUIRecord& rStats = m_ui.MarkerStats;
    rStats.Kind = addStatsRow(*markerGrid, markerRow, "Marker");
    rStats.FrameIndex = addStatsRow(*markerGrid, markerRow, "Frame index");
    rStats.AnimationTime = addStatsRow(*markerGrid, markerRow, "Animation time");
    rStats.RunId = addStatsRow(*markerGrid, markerRow, "Run id");
    rStats.IntendedDisplayTime = addStatsRow(*markerGrid, markerRow, "Intended display time");
    rStats.TargetFrameTime = addStatsRow(*markerGrid, markerRow, "Target frame time");
    rStats.CpuStartTime = addStatsRow(*markerGrid, markerRow, "CPU start time");
    rStats.CpuBusyTime = addStatsRow(*markerGrid, markerRow, "CPU busy");
    rStats.PreferredFrameTime = addStatsRow(*markerGrid, markerRow, "Preferred frame time");
    rStats.Static = addStatsRow(*markerGrid, markerRow, "Static");
    rStats.RunStartTime = addStatsRow(*markerGrid, markerRow, "Run start time");
    rStats.RunSequenceId = addStatsRow(*markerGrid, markerRow, "Sequence id");
    rStats.SyncMarker = addStatsRow(*markerGrid, markerRow, "Sync marker");

    // The frame times and the late frames are the ones of the last two seconds, the work and the present wait the ones of the last frame
    const auto pacerGrid = createGrid();
    uint32_t pacerRow = 0;
    PacerStatsUIRecord& rPacerStats = m_ui.PacerStats;
    if (m_explicitSync != SampleExplicitSync::NotApplicable)
    {
      // Only what is certain: if the compositor offers it. If the driver uses it can not be asked.
      const std::string_view explicitSyncText = SampleExplicitSyncUtil::ToDisplayString(m_explicitSync);
      rPacerStats.ExplicitSync = addStatsRow(*pacerGrid, pacerRow, "Explicit sync");
      rPacerStats.ExplicitSync->SetContent(StringViewLite(explicitSyncText.data(), explicitSyncText.size()));
    }
    rPacerStats.RefreshRate = addStatsRow(*pacerGrid, pacerRow, "Refresh rate");
    rPacerStats.PacedRate = addStatsRow(*pacerGrid, pacerRow, "Paced rate");
    rPacerStats.SwapInterval = addStatsRow(*pacerGrid, pacerRow, "Swap interval");
    rPacerStats.FrameTime = addStatsRow(*pacerGrid, pacerRow, "Frame time");
    rPacerStats.LateFrames = addStatsRow(*pacerGrid, pacerRow, "Late frames");
    rPacerStats.Work = addStatsRow(*pacerGrid, pacerRow, "Work");
    rPacerStats.AverageWork = addStatsRow(*pacerGrid, pacerRow, "Average work");
    rPacerStats.PresentWait = addStatsRow(*pacerGrid, pacerRow, "Present wait");
    rPacerStats.DisplaySideHolds = addStatsRow(*pacerGrid, pacerRow, "Display side holds");
    rPacerStats.IntervalChanges = addStatsRow(*pacerGrid, pacerRow, "Swap interval changes");
    rPacerStats.LastChange = addStatsRow(*pacerGrid, pacerRow, "Last change");
    rPacerStats.FrameWindow = addStatsRow(*pacerGrid, pacerRow, "Frame window");
    rPacerStats.Feedback = addStatsRow(*pacerGrid, pacerRow, "Present feedback");
    rPacerStats.FeedbackLate = addStatsRow(*pacerGrid, pacerRow, "Display late");
    rPacerStats.VariableRefresh = addStatsRow(*pacerGrid, pacerRow, "Variable refresh");
    // What the measured presents say: when the frames really reached the display
    rPacerStats.DisplayError = addStatsRow(*pacerGrid, pacerRow, "Display error");
    rPacerStats.DisplayInterval = addStatsRow(*pacerGrid, pacerRow, "Display interval");
    rPacerStats.AnimationError = addStatsRow(*pacerGrid, pacerRow, "Animation error");
    rPacerStats.Latency = addStatsRow(*pacerGrid, pacerRow, "Latency");
    rPacerStats.TimedFrames = addStatsRow(*pacerGrid, pacerRow, "Timed frames");
    rPacerStats.GpuWork = addStatsRow(*pacerGrid, pacerRow, "GPU work");
    rPacerStats.DisplayRefresh = addStatsRow(*pacerGrid, pacerRow, "Swapchain refresh");

    // Each overlay is a dialog window of the theme: a caption, a divider and the grid
    const auto createOverlay = [&rUIFactory](const char* const pszCaption, const std::shared_ptr<UI::GridLayout>& grid)
    {
      const auto content = std::make_shared<UI::StackLayout>(rUIFactory.GetContext());
      content->SetOrientation(UI::LayoutOrientation::Vertical);
      content->AddChild(rUIFactory.CreateLabel(pszCaption));
      content->AddChild(rUIFactory.CreateDivider(UI::LayoutOrientation::Horizontal));
      content->AddChild(grid);
      auto overlay = rUIFactory.CreateBackgroundWindow(UI::Theme::WindowType::DialogTransparent, content);
      overlay->SetAlignmentX(UI::ItemAlignment::Far);
      return overlay;
    };
    m_ui.MarkerStatsOverlay = createOverlay("Last marker", markerGrid);
    m_ui.PacerStatsOverlay = createOverlay("Frame pacing", pacerGrid);

    // The overlays are stacked at the top right, next to the right bar
    auto overlayStack = std::make_shared<UI::StackLayout>(rUIFactory.GetContext());
    overlayStack->SetOrientation(UI::LayoutOrientation::Vertical);
    overlayStack->SetAlignmentX(UI::ItemAlignment::Far);
    overlayStack->SetAlignmentY(UI::ItemAlignment::Near);
    overlayStack->AddChild(m_ui.MarkerStatsOverlay);
    overlayStack->AddChild(m_ui.PacerStatsOverlay);
    return overlayStack;
  }


  std::shared_ptr<UI::BaseWindow> FramePacingShared::CreateWorkChartBar(UI::Theme::IThemeControlFactory& rUIFactory)
  {
    const auto context = rUIFactory.GetContext();

    // What every frame cost: how long the CPU worked on it, how long the GPU did, and how long the frame took, which is to the end
    // of the last work on it. They are three values that are measured from the start of the frame, not parts of a sum, so the chart
    // draws each as a line of its own (the data says so, ChartChannelPolicy::Overlaid).
    const auto chart = std::make_shared<UI::AreaChart>(context);
    chart->SetAlignmentX(UI::ItemAlignment::Stretch);
    chart->SetAlignmentY(UI::ItemAlignment::Stretch);
    chart->SetOpaqueFillSprite(rUIFactory.GetResources().GetBasicFillSprite(true));
    chart->SetTransparentFillSprite(rUIFactory.GetResources().GetBasicFillSprite(false));
    chart->SetGridLines(std::make_unique<UI::ChartGridLinesFps>());
    chart->SetDataView(m_workChartData);
    chart->SetFont(rUIFactory.GetResources().GetDefaultSpriteFont());
    chart->SetLabelBackground(rUIFactory.GetResources().GetToolTipNineSliceSprite());
    chart->SetRenderPolicy(UI::ChartRenderPolicy::FillAvailable);

    // The legend
    m_ui.LabelWorkCpu = rUIFactory.CreateLabel("CPU");
    m_ui.LabelWorkCpu->SetFontColor(WorkChartCpuColor);
    m_ui.LabelWorkGpu = rUIFactory.CreateLabel("GPU");
    m_ui.LabelWorkGpu->SetFontColor(WorkChartGpuColor);
    m_ui.LabelWorkFrame = rUIFactory.CreateLabel("Frame");
    m_ui.LabelWorkFrame->SetFontColor(WorkChartFrameColor);
    const auto legend = std::make_shared<UI::StackLayout>(context);
    legend->SetOrientation(UI::LayoutOrientation::Vertical);
    legend->SetAlignmentY(UI::ItemAlignment::Center);
    legend->AddChild(rUIFactory.CreateLabel("Work per frame (average)"));
    legend->AddChild(m_ui.LabelWorkFrame);
    legend->AddChild(m_ui.LabelWorkGpu);
    legend->AddChild(m_ui.LabelWorkCpu);

    const auto grid = std::make_shared<UI::GridLayout>(context);
    grid->SetAlignmentX(UI::ItemAlignment::Stretch);
    grid->SetMargin(DpThicknessF::Create(8, 0, 8, 0));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Fixed, ChartLegendWidthDp));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Fixed, 8));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    grid->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Fixed, WorkChartHeightDp));
    grid->AddChild(legend, 0, 0);
    grid->AddChild(chart, 2, 0);
    return rUIFactory.CreateBottomBar(grid, UI::Theme::BarType::Transparent);
  }


  std::shared_ptr<UI::BaseWindow> FramePacingShared::CreateAnimationErrorChartBar(UI::Theme::IThemeControlFactory& rUIFactory)
  {
    const auto context = rUIFactory.GetContext();

    // The animation error of every frame, drawn as the report of the mb-framepacing tools draws it (see SampleAnimationErrorChart)
    m_ui.AnimationErrorChart = std::make_shared<SampleAnimationErrorChart>(context);
    m_ui.AnimationErrorChart->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.AnimationErrorChart->SetAlignmentY(UI::ItemAlignment::Stretch);
    m_ui.AnimationErrorChart->SetFillSprite(rUIFactory.GetResources().GetBasicFillSprite(false));

    // The labels of the two dashed lines. The scale is two refreshes to each side, so the lines are a quarter of the height from
    // the top and from the bottom: the label of the upper one is above it, the one of the lower one below it, as in the report
    m_ui.LabelAnimationErrorEarly = rUIFactory.CreateLabel("+1 refresh");
    m_ui.LabelAnimationErrorEarly->SetFontColor(AnimationErrorChartRefreshColor);
    m_ui.LabelAnimationErrorEarly->SetAlignmentX(UI::ItemAlignment::Far);
    m_ui.LabelAnimationErrorEarly->SetAlignmentY(UI::ItemAlignment::Far);
    m_ui.LabelAnimationErrorLate = rUIFactory.CreateLabel("-1 refresh");
    m_ui.LabelAnimationErrorLate->SetFontColor(AnimationErrorChartRefreshColor);
    m_ui.LabelAnimationErrorLate->SetAlignmentX(UI::ItemAlignment::Far);
    m_ui.LabelAnimationErrorLate->SetAlignmentY(UI::ItemAlignment::Near);
    const auto labelGrid = std::make_shared<UI::GridLayout>(context);
    labelGrid->SetAlignmentX(UI::ItemAlignment::Stretch);
    labelGrid->SetAlignmentY(UI::ItemAlignment::Stretch);
    labelGrid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    labelGrid->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    labelGrid->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 2.0f));
    labelGrid->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    labelGrid->AddChild(m_ui.LabelAnimationErrorEarly, 0, 0);
    labelGrid->AddChild(m_ui.LabelAnimationErrorLate, 0, 2);

    // The legend says where the display times are from: they are what the system reports and not a measurement of the display
    const auto labelError = rUIFactory.CreateLabel("Animation error");
    labelError->SetFontColor(AnimationErrorChartBarColor);
    const auto legend = std::make_shared<UI::StackLayout>(context);
    legend->SetOrientation(UI::LayoutOrientation::Vertical);
    legend->SetAlignmentY(UI::ItemAlignment::Center);
    // What the chart shows is counted, in three numbers: the stutter (the frames with a error over the threshold) of the last second,
    // and the stutter of the run by its two causes as the mb-framepacing tools tell them apart (jitter: their delta time jitter)
    m_ui.LabelStutterRecent = rUIFactory.CreateLabel("Last second: 0");
    m_ui.LabelStutterRun = rUIFactory.CreateLabel("Run: 0 pacing, 0 jitter");
    legend->AddChild(labelError);
    legend->AddChild(m_ui.LabelStutterRecent);
    legend->AddChild(m_ui.LabelStutterRun);

    const auto grid = std::make_shared<UI::GridLayout>(context);
    grid->SetAlignmentX(UI::ItemAlignment::Stretch);
    grid->SetMargin(DpThicknessF::Create(8, 0, 8, 0));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Fixed, ChartLegendWidthDp));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Fixed, 8));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    grid->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Fixed, AnimationErrorChartHeightDp));
    grid->AddChild(legend, 0, 0);
    grid->AddChild(m_ui.AnimationErrorChart, 2, 0);
    // In the same cell as the chart, on top of it
    grid->AddChild(labelGrid, 2, 0);
    return rUIFactory.CreateBottomBar(grid, UI::Theme::BarType::Transparent);
  }


  void FramePacingShared::UpdateAnimationError()
  {
    const TimeSpan refreshPeriod = NanosecondTimeSpanUtil::ToTimeSpan(GetRefreshPeriod());
    if (refreshPeriod != m_animationErrorRefreshPeriod)
    {
      // The chart is in refreshes: what a refresh is in time is said by the labels of its lines, and the band of no error is a time
      m_animationErrorRefreshPeriod = refreshPeriod;
      m_ui.AnimationErrorChart->SetNoErrorBand(SampleAnimationError::ToRefreshThousandths(SampleAnimationError::ErrorThreshold, refreshPeriod));
      if (refreshPeriod.Ticks() > 0)
      {
        SetFormattedContent(*m_ui.LabelAnimationErrorEarly, "+1 refresh ({:.1f} ms)", refreshPeriod.TotalMilliseconds());
        SetFormattedContent(*m_ui.LabelAnimationErrorLate, "-1 refresh ({:.1f} ms)", refreshPeriod.TotalMilliseconds());
      }
      else
      {
        m_ui.LabelAnimationErrorEarly->SetContent("+1 refresh");
        m_ui.LabelAnimationErrorLate->SetContent("-1 refresh");
      }
    }

    // The frames whose display times have come by now: they come a few frames after the frame, so the chart is that far behind
    const bool isChartShown = m_ui.SwitchAnimationErrorChart->IsChecked();
    SampleAnimationErrorRecord record;
    while (m_animationError.TryPop(record))
    {
      if (isChartShown)
      {
        if (record.IsJudged)
        {
          m_ui.AnimationErrorChart->AddError(SampleAnimationError::ToRefreshThousandths(record.Error, refreshPeriod));
        }
        else
        {
          m_ui.AnimationErrorChart->AddGap();
        }
      }
      if (record.IsJudged && m_trace)
      {
        // In the row of the frame it is the error of
        const LogPresentFrame& presentFrame = m_logPresentFrames[record.PresentId % m_logPresentFrames.size()];
        if (presentFrame.PresentId == record.PresentId)
        {
          m_trace->SetValueAt(presentFrame.FrameIndex, m_logColumns.AnimationError, record.Error);
        }
      }
    }

    const TickCount now = m_timer.GetTimestamp();
    if ((now - m_stutterLegendTime) >= ChartLegendInterval)
    {
      // The legend, written a few times a second so it can be read
      m_stutterLegendTime = now;
      const SampleStutterCount runStutter = m_animationError.GetRunStutter();
      SetFormattedContent(*m_ui.LabelStutterRecent, "Last second: {}", m_animationError.CalcRecentStutter().Total());
      SetFormattedContent(*m_ui.LabelStutterRun, "Run: {} pacing, {} jitter", runStutter.Pacing, runStutter.DeltaTimeJitter);
    }
  }


  void FramePacingShared::UpdateStatsVisibility()
  {
    const auto setVisible = [](UI::BaseWindow& rWindow, const bool visible)
    {
      const UI::ItemVisibility visibility = visible ? UI::ItemVisibility::Visible : UI::ItemVisibility::Collapsed;
      if (rWindow.GetVisibility() != visibility)
      {
        rWindow.SetVisibility(visibility);
      }
    };
    setVisible(*m_ui.MarkerStatsOverlay, m_ui.SwitchMarkerStats->IsChecked());
    setVisible(*m_ui.PacerStatsOverlay, m_ui.SwitchPacerStats->IsChecked());
    setVisible(*m_ui.WorkChartBar, m_ui.SwitchWorkChart->IsChecked());
    // There is only something to draw where the app is told when its frames were shown
    setVisible(*m_ui.AnimationErrorChartBar, m_ui.SwitchAnimationErrorChart->IsChecked() && m_presentFeedbackEnabled);
  }


  void FramePacingShared::UpdateSyncMarker()
  {
    if (m_framePacing && m_ui.SwitchSyncMarker->IsChecked() != m_framePacing->IsSyncMarkerEnabled())
    {
      m_framePacing->SetSyncMarkerEnabled(m_ui.SwitchSyncMarker->IsChecked());
    }
  }


  void FramePacingShared::UpdateMarkerStats()
  {
    if (!m_ui.SwitchMarkerStats->IsChecked())
    {
      // The overlay is hidden. The steps from the marker before start again when it is shown.
      m_markerStep = {};
      return;
    }
    const MarkerStatsUIRecord& rStats = m_ui.MarkerStats;
    FramePacingMarkerInfo info;
    if (!m_framePacing || !m_framePacing->TryGetLastMarker(info))
    {
      rStats.Kind->SetContent("none drawn yet");
      for (UI::Label* pLabel : {rStats.FrameIndex.get(), rStats.AnimationTime.get(), rStats.RunId.get(), rStats.IntendedDisplayTime.get(),
                                rStats.TargetFrameTime.get(), rStats.CpuStartTime.get(), rStats.CpuBusyTime.get(), rStats.PreferredFrameTime.get(),
                                rStats.Static.get(), rStats.RunStartTime.get(), rStats.RunSequenceId.get(), rStats.SyncMarker.get()})
      {
        pLabel->SetContent(UnknownValue);
      }
      m_markerStep = {};
      return;
    }

    // The panel is updated every frame, so a new marker is the one that follows the marker it showed last
    if (!m_markerStep.HasMarker || info.FrameIndex != m_markerStep.FrameIndex)
    {
      m_markerStep.AnimationStep.reset();
      m_markerStep.IntendedDisplayStep.reset();
      m_markerStep.CpuStartStep.reset();
      if (m_markerStep.HasMarker)
      {
        m_markerStep.AnimationStep = info.AnimationTime - m_markerStep.AnimationTime;
        if (info.IntendedDisplayTime.has_value() && m_markerStep.IntendedDisplayTime.has_value())
        {
          m_markerStep.IntendedDisplayStep = info.IntendedDisplayTime.value() - m_markerStep.IntendedDisplayTime.value();
        }
        if (info.CpuStartTime.has_value() && m_markerStep.CpuStartTime.has_value())
        {
          m_markerStep.CpuStartStep = info.CpuStartTime.value() - m_markerStep.CpuStartTime.value();
        }
      }
      m_markerStep.HasMarker = true;
      m_markerStep.FrameIndex = info.FrameIndex;
      m_markerStep.AnimationTime = info.AnimationTime;
      m_markerStep.IntendedDisplayTime = info.IntendedDisplayTime;
      m_markerStep.CpuStartTime = info.CpuStartTime;
    }

    rStats.Kind->SetContent(ToString(info.Kind));
    SetFormattedContent(*rStats.FrameIndex, "{}", info.FrameIndex);
    // The times are followed by their step from the marker before: the delta time (DT)
    if (m_markerStep.AnimationStep.has_value())
    {
      SetFormattedContent(*rStats.AnimationTime, "{:.3f} ms (DT {:+.3f})", info.AnimationTime.TotalMilliseconds(),
                          m_markerStep.AnimationStep->TotalMilliseconds());
    }
    else
    {
      SetFormattedContent(*rStats.AnimationTime, "{:.3f} ms", info.AnimationTime.TotalMilliseconds());
    }
    SetFormattedContent(*rStats.RunId, "{}", info.RunId);
    if (info.IntendedDisplayTime.has_value() && m_markerStep.IntendedDisplayStep.has_value())
    {
      SetFormattedContent(*rStats.IntendedDisplayTime, "{:.3f} ms (DT {:+.3f})", info.IntendedDisplayTime->TotalMilliseconds(),
                          m_markerStep.IntendedDisplayStep->TotalMilliseconds());
    }
    else if (info.IntendedDisplayTime.has_value())
    {
      SetFormattedContent(*rStats.IntendedDisplayTime, "{:.3f} ms", info.IntendedDisplayTime->TotalMilliseconds());
    }
    else
    {
      rStats.IntendedDisplayTime->SetContent(UnknownValue);
    }
    if (info.TargetFrameTime.has_value())
    {
      SetFormattedContent(*rStats.TargetFrameTime, "{:.3f} ms", info.TargetFrameTime->TotalMilliseconds());
    }
    else
    {
      rStats.TargetFrameTime->SetContent(UnknownValue);
    }
    if (info.CpuStartTime.has_value() && m_markerStep.CpuStartStep.has_value())
    {
      SetFormattedContent(*rStats.CpuStartTime, "{:.3f} ms (DT {:+.3f})", info.CpuStartTime->TotalMilliseconds(),
                          m_markerStep.CpuStartStep->TotalMilliseconds());
    }
    else if (info.CpuStartTime.has_value())
    {
      SetFormattedContent(*rStats.CpuStartTime, "{:.3f} ms", info.CpuStartTime->TotalMilliseconds());
    }
    else
    {
      rStats.CpuStartTime->SetContent(UnknownValue);
    }
    if (info.CpuBusyTime.has_value())
    {
      SetFormattedContent(*rStats.CpuBusyTime, "{:.3f} ms", info.CpuBusyTime->TotalMilliseconds());
    }
    else
    {
      rStats.CpuBusyTime->SetContent(UnknownValue);
    }
    if (info.PreferredFrameTime.has_value())
    {
      SetFormattedContent(*rStats.PreferredFrameTime, "{:.3f} ms", info.PreferredFrameTime->TotalMilliseconds());
    }
    else
    {
      rStats.PreferredFrameTime->SetContent(UnknownValue);
    }
    // Static after: the app said nothing animates while the frame is on screen. Static before: the service found that the frame has the
    // animation time of the frame before it (the app is paused).
    rStats.Static->SetContent(info.Static ? (info.StaticBefore ? "after, before" : "after") : (info.StaticBefore ? "before" : "no"));
    // The run start time and the sequence id are only carried by start markers
    if (info.RunStartTime.has_value())
    {
      SetFormattedContent(*rStats.RunStartTime, "{:%Y-%m-%d %H:%M:%S} UTC", std::chrono::floor<std::chrono::seconds>(info.RunStartTime.value()));
    }
    else
    {
      rStats.RunStartTime->SetContent(UnknownValue);
    }
    if (info.RunSequenceId.has_value())
    {
      m_formatBuffer.clear();
      for (const uint8_t value : info.RunSequenceId->Bytes)
      {
        fmt::format_to(std::back_inserter(m_formatBuffer), "{:02x}", value);
      }
      rStats.RunSequenceId->SetContent(StringViewLite(m_formatBuffer.data(), m_formatBuffer.size()));
    }
    else
    {
      rStats.RunSequenceId->SetContent(UnknownValue);
    }
    rStats.SyncMarker->SetContent(info.SyncMarker ? "drawn" : "off");
  }


  void FramePacingShared::DrawBoxAnimation(const double animationSeconds)
  {
    if (!m_nativeBatch)
    {
      return;
    }
    const SampleBoxAnimationSpeed speed = m_ui.SwitchBoxAnimationFast->IsChecked() ? SampleBoxAnimationSpeed::Fast : SampleBoxAnimationSpeed::Normal;
    // The box moves in the part of the window that is left of the controls, so it is not hidden by them at the end of its path
    const int32_t controlsWidthPx = m_ui.RightBar ? m_ui.RightBar->RenderSizePx().RawWidth() : 0;
    const int32_t areaWidthPx = std::max(m_windowSizePx.RawWidth() - controlsWidthPx, 0);
    const PxRectangle boxRectanglePx =
      SampleBoxAnimation::CalcBoxRectangle(PxSize2D::Create(areaWidthPx, m_windowSizePx.RawHeight()), animationSeconds, speed);
    if (boxRectanglePx.RawWidth() <= 0)
    {
      return;
    }
    // Just the box, with nothing behind it but what the sample shows
    m_nativeBatch->Begin(BlendState::Opaque);
    m_nativeBatch->Draw(m_fillTexture, boxRectanglePx, Colors::White());
    m_nativeBatch->End();
  }


  void FramePacingShared::DrawAnimation(const double animationSeconds)
  {
    const PxSize2D windowSizePx = m_windowSizePx;
    if (!m_nativeBatch || windowSizePx.RawWidth() <= 0 || windowSizePx.RawHeight() <= 0)
    {
      return;
    }
    const auto width = static_cast<double>(windowSizePx.RawWidth());
    const auto height = windowSizePx.RawHeight();

    // Constant speed motion: any hitch or uneven frame pacing is easy to spot
    const double sweep = std::fmod(animationSeconds / LocalConfig::SweepSeconds, 1.0);
    const auto barX = static_cast<int32_t>(std::lround(sweep * (width - LocalConfig::BarWidthPx)));

    // A box that moves back and forth in the middle of the screen
    const double pingPong = 1.0 - std::abs((2.0 * std::fmod(animationSeconds / (LocalConfig::SweepSeconds * 2.0), 1.0)) - 1.0);
    const auto boxX = static_cast<int32_t>(std::lround(pingPong * (width - LocalConfig::BoxSizePx)));
    const int32_t boxY = (height - LocalConfig::BoxSizePx) / 2;

    m_nativeBatch->Begin(BlendState::Opaque);
    m_nativeBatch->Draw(m_fillTexture, PxRectangle::Create(barX, 0, LocalConfig::BarWidthPx, height), Colors::White());
    m_nativeBatch->Draw(m_fillTexture, PxRectangle::Create(boxX, boxY, LocalConfig::BoxSizePx, LocalConfig::BoxSizePx), Colors::Orange());
    m_nativeBatch->End();
  }
}
