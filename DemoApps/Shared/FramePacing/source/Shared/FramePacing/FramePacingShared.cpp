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
#include <FslDemoApp/Shared/Host/DemoWindowMetrics.hpp>
#include <FslDemoHost/Base/Service/WindowHost/IWindowHostInfo.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingFrameSchedule.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingMarkerInfo.hpp>
#include <FslDemoService/FramePacingMarker/IFramePacingFrameLog.hpp>
#include <FslDemoService/FramePacingMarker/IFramePacingMarkerService.hpp>
#include <FslDemoService/Graphics/IGraphicsService.hpp>
#include <FslGraphics/Bitmap/ReadOnlyRawBitmap.hpp>
#include <FslGraphics/Colors.hpp>
#include <FslGraphics/Render/Adapter/INativeBatch2D.hpp>
#include <FslNativeWindow/Base/INativeWindow.hpp>
#include <FslNativeWindow/Base/NativeWindowDisplayInfo.hpp>
#include <FslNativeWindow/Base/NativeWindowTimingSupport.hpp>
#include <FslBase/Time/NanosecondTickCountUtil.hpp>
#include <FslBase/Time/NanosecondTimeSpanUtil.hpp>
#include <FslNativeWindow/Base/NativeWindowVSyncInfo.hpp>
#include <FslNativeWindow/Base/NativeWindowVariableRefreshInfo.hpp>
#include <FslNativeWindow/Base/VirtualKey.hpp>
#include <FslSimpleUI/App/Theme/ThemeSelector.hpp>
#include <FslSimpleUI/Base/Control/Background.hpp>
#include <FslSimpleUI/Base/Control/Image.hpp>
#include <FslSimpleUI/Base/Control/ScrollViewer.hpp>
#include <FslSimpleUI/Base/Event/WindowSelectEvent.hpp>
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
#include <Shared/FramePacing/SamplePacingTierClassifier.hpp>
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
      //! WaitForPresent presents this long after the last refresh before the one the frame pacer aims for (at most an eighth of a refresh)
      constexpr TimeSpan MaxPresentMargin(TimeSpan::TicksPerMillisecond);
      //! How long after the first frame that waited for the time of the pacer the one-time wait of SampleConfig::DrainRefreshes is
      //! made. The presents that queue up do so in the first frames of a swapchain, while its images take longer to reach the display
      //! (on Windows the first presents of a window go through the compositor), so the wait comes after those.
      constexpr TimeSpan DrainDelay(TimeSpan::TicksPerSecond / 2);
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

    const char* ToString(const SamplePacerHold hold) noexcept
    {
      switch (hold)
      {
      case SamplePacerHold::Auto:
        return "auto";
      case SamplePacerHold::VSync:
        return "vsync";
      case SamplePacerHold::Schedule:
        return "schedule";
      case SamplePacerHold::Wait:
      default:
        return "wait";
      }
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

    //! The value of the holdMethod column of the frame log
    int64_t ToLogCode(const SamplePacerHold hold) noexcept
    {
      switch (hold)
      {
      case SamplePacerHold::VSync:
        return 1;
      case SamplePacerHold::Schedule:
        return 3;
      case SamplePacerHold::Auto:
      case SamplePacerHold::Wait:
      default:
        return 0;
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
    constexpr UI::UIColor WorkChartFrameColor(PackedColor32(0xFF8C8C8C));    // grey
    constexpr float WorkChartHeightDp = 100.0f;

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
    m_pacerProfile = options->GetPacerProfile();
    // Only a app whose present is a swap has a use for it (SamplePresentMethod::SwapInterval), a Vulkan app submits its frame
    m_flushWanted = options->IsGLFlushEnabled() && presentMethod == SamplePresentMethod::SwapInterval;
    m_refreshRateOverrideHz = options->GetPacerRefreshRateHz();
    m_frameLog = config.DemoServiceProvider.TryGet<IFramePacingFrameLog>();
    if (m_frameLog && m_frameLog->IsLogEnabled())
    {
      RegisterLogColumns();
    }
    else
    {
      m_frameLog.reset();
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
        m_explicitSync = SamplePacingTierClassifier::ToExplicitSync(timingSupport);
        m_vsyncSourceName = timingSupport.VSyncSource;
      }
    }
    m_detectedRefreshRateHz = ReadDisplayRefreshRateHz();

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
    m_ui.LabelPacedRate = uiFactory->CreateLabel("");
    const auto lblTargetFps = uiFactory->CreateLabel("Target fps (0 = display rate)");
    m_ui.SliderTargetFps =
      uiFactory->CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, WithValue(SampleConfig::TargetFps, options->GetPacerTargetFps()));
    m_ui.SliderTargetFps->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.SwitchAdaptive = uiFactory->CreateSwitch("Adaptive swap interval", options->IsPacerAdaptive());
    // Only for an app that measures its presents, so it stays disabled until the pacer is on and the presents are measured
    m_ui.SwitchPacerFeedback = uiFactory->CreateSwitch("Present feedback to the pacer", options->IsPacerPresentFeedback());
    m_ui.SwitchPacerFeedback->SetEnabled(false);
    // How a frame is held for more than one refresh. A method the system can not do falls back to one it can (GetHoldMethod).
    const SamplePacerHold hold = options->GetPacerHold();
    const auto holdGroup = uiFactory->CreateRadioGroup("hold");
    m_ui.RadioHoldAuto = uiFactory->CreateRadioButton(holdGroup, "Hold: auto", hold == SamplePacerHold::Auto);
    m_ui.RadioHoldVSync = uiFactory->CreateRadioButton(holdGroup, "Hold: wait on the vsync", hold == SamplePacerHold::VSync);
    m_ui.RadioHoldWait = uiFactory->CreateRadioButton(holdGroup, "Hold: sleep", hold == SamplePacerHold::Wait);
    m_ui.RadioHoldSchedule = uiFactory->CreateRadioButton(holdGroup, "Hold: scheduled present", hold == SamplePacerHold::Schedule);
    m_vsyncPhasePercent = options->GetPacerVSyncPhasePercent();
    m_drainRefreshes = options->GetPacerDrainRefreshes();
    // How well a frame is held for more than one refresh (Doc/FramePacingPlatformSupport.md), see UpdateTier
    m_ui.LabelTierInUse = uiFactory->CreateLabel("");
    m_ui.LabelTierBest = uiFactory->CreateLabel("");
    m_ui.LabelPacerStatus = uiFactory->CreateLabel("");
    m_ui.LabelPacerFrames = uiFactory->CreateLabel("");
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

    const auto stackLayout = std::make_shared<UI::StackLayout>(uiFactory->GetContext());
    stackLayout->SetOrientation(UI::LayoutOrientation::Vertical);
    stackLayout->SetAlignmentY(UI::ItemAlignment::Center);
    stackLayout->AddChild(m_ui.LabelStatus);
    stackLayout->AddChild(m_ui.LabelRun);
    stackLayout->AddChild(m_ui.ButtonRun);
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(lblDuration);
    stackLayout->AddChild(m_ui.SliderDuration);
    stackLayout->AddChild(m_ui.ButtonTimedRun);
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(m_ui.SwitchPacer);
    stackLayout->AddChild(m_ui.LabelTierInUse);
    stackLayout->AddChild(m_ui.LabelTierBest);
    stackLayout->AddChild(m_ui.LabelRefreshRate);
    stackLayout->AddChild(m_ui.LabelPacedRate);
    stackLayout->AddChild(m_ui.SliderRefreshRate);
    stackLayout->AddChild(lblTargetFps);
    stackLayout->AddChild(m_ui.SliderTargetFps);
    stackLayout->AddChild(m_ui.SwitchAdaptive);
    stackLayout->AddChild(m_ui.SwitchPacerFeedback);
    stackLayout->AddChild(m_ui.RadioHoldAuto);
    stackLayout->AddChild(m_ui.RadioHoldVSync);
    stackLayout->AddChild(m_ui.RadioHoldWait);
    stackLayout->AddChild(m_ui.RadioHoldSchedule);
    stackLayout->AddChild(m_ui.LabelPacerStatus);
    stackLayout->AddChild(m_ui.LabelPacerFrames);
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(lblCpuLoad);
    stackLayout->AddChild(m_ui.SliderCpuLoad);
    stackLayout->AddChild(lblGpuLoad);
    stackLayout->AddChild(m_ui.SliderGpuLoad);
    stackLayout->AddChild(m_ui.RadioBackgroundMandelbrot);
    stackLayout->AddChild(m_ui.RadioBackgroundBlobs);
    stackLayout->AddChild(m_ui.RadioBackgroundLace);
    stackLayout->AddChild(m_ui.RadioBackgroundFlight);
    stackLayout->AddChild(m_ui.RadioBackgroundHall);
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(m_ui.SwitchMarkerStats);
    stackLayout->AddChild(m_ui.SwitchPacerStats);
    stackLayout->AddChild(m_ui.SwitchWorkChart);
    stackLayout->AddChild(m_ui.SwitchAnimationErrorChart);
    stackLayout->AddChild(m_ui.SwitchTestPattern);
    stackLayout->AddChild(m_ui.SwitchBoxAnimation);
    stackLayout->AddChild(m_ui.SwitchBoxAnimationFast);
    stackLayout->AddChild(m_ui.SwitchSyncMarker);
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(m_ui.SwitchPresentTiming);
    stackLayout->AddChild(m_ui.SwitchGpuTimeline);
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(lblHint);
    stackLayout->AddChild(lblHintTimed);
    stackLayout->AddChild(lblHintPacer);

    const auto mainLayout = std::make_shared<UI::GridLayout>(uiFactory->GetContext());
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    mainLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    // Left of the right bar: the overlays with every value of the last marker and the frame pacing stats, above the chart of the work
    // per frame. Each is shown while its switch is on.
    const auto contentLayout = std::make_shared<UI::GridLayout>(uiFactory->GetContext());
    contentLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    contentLayout->SetAlignmentY(UI::ItemAlignment::Stretch);
    contentLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    contentLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    contentLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Auto));
    contentLayout->AddChild(CreateStatsWindow(*uiFactory), 0, 0);
    m_ui.WorkChartBar = CreateWorkChartBar(*uiFactory);
    m_ui.AnimationErrorChartBar = CreateAnimationErrorChartBar(*uiFactory);
    {
      // The two charts, each can be hidden
      const auto chartLayout = std::make_shared<UI::StackLayout>(uiFactory->GetContext());
      chartLayout->SetOrientation(UI::LayoutOrientation::Vertical);
      chartLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
      chartLayout->AddChild(m_ui.AnimationErrorChartBar);
      chartLayout->AddChild(m_ui.WorkChartBar);
      contentLayout->AddChild(chartLayout, 0, 1);
    }
    mainLayout->AddChild(contentLayout, 0, 0);
    // The controls can be scrolled, as a low window does not have room for all of them
    stackLayout->SetMargin(DpThicknessF::Create(0, 0, 8, 0));
    const auto scrollViewer = uiFactory->CreateScrollViewer(stackLayout, UI::ScrollModeFlags::TranslateY, false);
    const auto rightBar = uiFactory->CreateRightBar(scrollViewer);
    mainLayout->AddChild(rightBar, 1, 0);
    m_ui.RightBar = rightBar;
    mainLayout->SetLimitToAvailableSpace(true);
    m_uiExtension->SetMainWindow(mainLayout);

    UpdateUI();
    UpdateStatsVisibility();
    UpdateMarkerStats();
    UpdateRefreshRateUI();
    UpdatePacerStatus();
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


  void FramePacingShared::OnKeyEvent(const KeyEvent& event)
  {
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
    UpdatePacer();
    UpdateTier();
    if (m_presentMethod == SamplePresentMethod::SwapInterval)
    {
      // The frame starts here: the host updates the app right after the swap of the previous frame, which waited for the display
      StartFrame();
    }

    UpdatePacerStatus();
    UpdateStatsVisibility();
    UpdateSyncMarker();
    UpdatePacerStats();
    UpdateMarkerStats();
    {
      // The frames everything is known about by now: the GPU time of a frame comes a frame or more after the frame ended, so the
      // chart is that far behind
      SampleFrameWorkRecord work;
      while (m_frameWork.TryPop(work))
      {
        if (m_ui.SwitchWorkChart->IsChecked())
        {
          UI::ChartDataEntry entry;
          entry.Values[WorkChartCpuChannel] = ToChartMicroseconds(work.CpuTime);
          entry.Values[WorkChartGpuChannel] = ToChartMicroseconds(work.GpuTime);
          entry.Values[WorkChartFrameChannel] = ToChartMicroseconds(work.FrameTime);
          m_workChartData->Append(entry);
        }
      }
    }
    UpdateAnimationError();
    if (m_framePacing)
    {
      const int64_t measuredTenths = m_framePacing->GetRunMeasuredTime().Ticks() / (TimeSpan::TicksPerMillisecond * 100);
      if (m_framePacing->GetRunState() != m_cachedRunState || m_framePacing->GetRunId() != m_cachedRunId || measuredTenths != m_cachedMeasuredTenths)
      {
        UpdateUI();
      }
    }
  }


  void FramePacingShared::Draw()
  {
    // SamplePresentMethod::WaitThenPresent: the frame starts here, after the host waited for a free buffer
    StartFrame();

    if (m_frameLog)
    {
      // A frame has its row in the log from the draw on. A frame that starts in the update (SamplePresentMethod::SwapInterval) starts
      // before that, and what is written then lands in the row of the frame before. So what the frame started with is written here.
      m_logFrames[m_frameId % m_logFrames.size()] = {m_frameId, m_frameLog->GetLogFrameIndex()};
      if (m_frameStartLogPending)
      {
        m_frameStartLogPending = false;
        LogFrameStart(m_frameStartLogWaitTime);
      }
    }

    // Tell the marker what the frame was paced by. Until the frame pacer was used the animation time is the time of the framework,
    // which the marker reports by itself.
    if (m_framePacing && (m_pacer || m_animationOffset.Ticks() != 0))
    {
      FramePacingFrameSchedule frameSchedule;
      frameSchedule.AnimationTime = m_animationTime;
      if (m_pacer)
      {
        frameSchedule.CpuStartTime = m_frameStartTime;
        frameSchedule.IntendedDisplayTime = m_schedule.IntendedDisplayTime;
        frameSchedule.TargetFrameTime = m_schedule.TargetFrameTime;
        frameSchedule.PreferredFrameTime = m_schedule.PreferredFrameTime;
      }
      m_framePacing->SetFrameSchedule(frameSchedule);
    }

    if (m_ui.SwitchTestPattern->IsChecked())
    {
      // Use the exact time the frame is animated for, this is what the marker reports
      DrawAnimation(m_animationTime.TotalSeconds());
    }
    if (m_ui.SwitchBoxAnimation->IsChecked())
    {
      // The same time as the test pattern, drawn on top of it
      DrawBoxAnimation(m_animationTime.TotalSeconds());
    }

    m_uiExtension->Draw();
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


  SamplePacerHold FramePacingShared::GetRequestedHold() const
  {
    if (m_ui.RadioHoldAuto->IsChecked())
    {
      return SamplePacerHold::Auto;
    }
    if (m_ui.RadioHoldVSync->IsChecked())
    {
      return SamplePacerHold::VSync;
    }
    return m_ui.RadioHoldSchedule->IsChecked() ? SamplePacerHold::Schedule : SamplePacerHold::Wait;
  }


  SamplePacerHold FramePacingShared::GetHoldMethod() const
  {
    const SamplePacerHold requested = GetRequestedHold();
    if (!m_pacer || requested == SamplePacerHold::Wait)
    {
      return SamplePacerHold::Wait;
    }
    if ((requested == SamplePacerHold::Schedule || requested == SamplePacerHold::Auto) && m_presentSchedulingSupported)
    {
      return SamplePacerHold::Schedule;
    }
    // Auto without a present that takes a target time, and what is left of a method the system can not do: the vsync wait, else the sleep.
    // The vertical blanks of a display that refreshes at a variable rate follow the frames, so there the vsync wait can not hold one.
    return (m_vsyncTime.Ticks() > 0 && m_vsyncPeriod.Ticks() > 0 && !m_variableRefreshSeen) ? SamplePacerHold::VSync : SamplePacerHold::Wait;
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
    const bool hasIntendedDisplayTime = m_pacer && m_schedule.IntendedDisplayTime.Ticks() != 0;
    m_presentFeedback.AddFrame(presentId, m_frameStartTime,
                               hasIntendedDisplayTime ? std::optional<TickCount>(m_schedule.IntendedDisplayTime) : std::nullopt);
    if (m_presentFeedbackEnabled)
    {
      // The moment the frame is drawn for, to be held against the time it is shown at
      m_animationError.AddFrame(presentId, m_animationTime);
    }

    {    // So the frame pacer can be told about the frame when its display time arrives, a few frames from now
      PacerPresentFrame& rPresentFrame = m_pacerPresentFrames[presentId % m_pacerPresentFrames.size()];
      rPresentFrame = {};
      rPresentFrame.PresentId = presentId;
      rPresentFrame.PacerFrameId = m_pacer ? m_schedule.FrameId : 0u;
      if (m_frameLog)
      {
        rPresentFrame.LogFrameIndex = m_frameLog->GetLogFrameIndex();
        rPresentFrame.HasLogFrame = true;
      }
    }

    // So something that is measured later about this present (when the GPU worked on it) finds the frame of the sample
    m_presentFrames[presentId % m_presentFrames.size()] = {presentId, m_frameId};
    if (m_frameLog)
    {
      // And the frame of the log
      m_logPresentFrames[presentId % m_logPresentFrames.size()] = {presentId, m_frameLog->GetLogFrameIndex()};
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

    // The frame pacer is told when the frame was shown, if it measures the frames by that. A present the presentation engine is done
    // with and has no display time for is reported as not shown, so the frame pacer does not take the frame to be on time.
    if (m_pacer && m_pacerConfig.PresentFeedback && !displayTime.has_value() && isComplete)
    {
      const PacerPresentFrame& presentFrame = m_pacerPresentFrames[presentId % m_pacerPresentFrames.size()];
      if (presentFrame.PresentId == presentId && presentFrame.PacerFrameId != 0u)
      {
        m_pacer->AddPresentNotShown(presentFrame.PacerFrameId);
        if (m_frameLog && presentFrame.HasLogFrame)
        {
          m_frameLog->SetLogInt64At(presentFrame.LogFrameIndex, m_logColumns.FeedbackReportedNotShown, 1);
        }
      }
    }
    if (m_pacer && m_pacerConfig.PresentFeedback && displayTime.has_value())
    {
      const PacerPresentFrame& presentFrame = m_pacerPresentFrames[presentId % m_pacerPresentFrames.size()];
      if (presentFrame.PresentId == presentId && presentFrame.PacerFrameId != 0u)
      {
        const std::optional<TickCount> presentCallTime =
          presentFrame.HasPresentCallTime ? std::optional<TickCount>(presentFrame.PresentCallTime) : std::nullopt;
        m_pacer->AddPresentFeedback(presentFrame.PacerFrameId, displayTime.value(), presentCallTime);
        if (m_frameLog && presentFrame.HasLogFrame)
        {
          // What the frame pacer was given about the frame
          m_frameLog->SetLogValueAt(presentFrame.LogFrameIndex, m_logColumns.FeedbackDisplay, displayTime.value());
          if (presentCallTime.has_value())
          {
            m_frameLog->SetLogValueAt(presentFrame.LogFrameIndex, m_logColumns.FeedbackPresent, presentCallTime.value());
          }
        }
      }
    }
  }


  void FramePacingShared::SetPresentCallTime(const uint64_t presentId, const TickCount presentCallTime)
  {
    PacerPresentFrame& rPresentFrame = m_pacerPresentFrames[presentId % m_pacerPresentFrames.size()];
    if (rPresentFrame.PresentId == presentId)
    {
      rPresentFrame.PresentCallTime = presentCallTime;
      rPresentFrame.HasPresentCallTime = true;
    }
  }


  void FramePacingShared::AddGpuInterval(const uint64_t presentId, const TickCount gpuStartTime, const TickCount gpuEndTime)
  {
    m_presentFeedback.AddGpuInterval(presentId, gpuStartTime, gpuEndTime);
    {
      const PresentFrame& presentFrame = m_presentFrames[presentId % m_presentFrames.size()];
      if (presentFrame.PresentId == presentId && presentFrame.FrameId != 0u)
      {
        m_frameWork.AddGpuInterval(presentFrame.FrameId, gpuStartTime, gpuEndTime);
      }
    }
    if (m_frameLog)
    {
      const LogPresentFrame& presentFrame = m_logPresentFrames[presentId % m_logPresentFrames.size()];
      if (presentFrame.PresentId == presentId)
      {
        m_frameLog->SetLogValueAt(presentFrame.FrameIndex, m_logColumns.GpuWorkBegin, gpuStartTime);
        m_frameLog->SetLogValueAt(presentFrame.FrameIndex, m_logColumns.GpuWorkEnd, gpuEndTime);
      }
    }
  }


  void FramePacingShared::AddGpuTime(const uint64_t frameId, const TimeSpan gpuTime, const std::optional<TickCount> gpuEndTime)
  {
    m_frameWork.AddGpuTime(frameId, gpuTime, gpuEndTime);
    m_gpuTimeLogFrameIndex.reset();
    if (m_frameLog)
    {
      const LogFrame& logFrame = m_logFrames[frameId % m_logFrames.size()];
      if (frameId != 0u && logFrame.FrameId == frameId)
      {
        // The time is written to the frame it was measured on, which is a frame or more before the one that is being drawn
        m_frameLog->SetLogValueAt(logFrame.FrameIndex, m_logColumns.GpuTime, gpuTime);
        if (gpuEndTime.has_value())
        {
          m_frameLog->SetLogValueAt(logFrame.FrameIndex, m_logColumns.GpuWorkEnd, gpuEndTime.value());
        }
        m_gpuTimeLogFrameIndex = logFrame.FrameIndex;
      }
    }
  }


  void FramePacingShared::AddGpuClockCalibration(const TimeSpan readTime)
  {
    if (m_frameLog)
    {
      m_frameLog->AddLogEvent("gpuClockCalibration", fmt::format("readTicks={};maxDeviationTicks={}", readTime.Ticks(), (readTime.Ticks() + 1) / 2));
    }
  }


  void FramePacingShared::AddGpuClockCalibration(const TimeSpan readTime, const TimeSpan maxDeviation,
                                                 const std::optional<double> clockRateDeviationPpm)
  {
    if (m_frameLog)
    {
      // The rate is left out until it was measured: the period the device states is used until then
      m_frameLog->AddLogEvent("gpuClockCalibration", clockRateDeviationPpm.has_value()
                                                       ? fmt::format("readTicks={};maxDeviationTicks={};clockRateDeviationPpm={:.2f}",
                                                                     readTime.Ticks(), maxDeviation.Ticks(), clockRateDeviationPpm.value())
                                                       : fmt::format("readTicks={};maxDeviationTicks={}", readTime.Ticks(), maxDeviation.Ticks()));
    }
  }


  void FramePacingShared::MarkFlush()
  {
    if (m_frameLog)
    {
      m_frameLog->SetLogValue(m_logColumns.FlushCall, m_timer.GetTimestamp());
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
    const TickCount now = m_timer.GetTimestamp();
    m_lastCpuTime = now - m_frameStartTime;
    m_lastGpuTime = TimeSpan(std::max(gpuTime.Ticks(), int64_t{0}));
    m_lastPresentWait = {};
    m_frameWork.EndCpuWork(now);
    m_tierFactsReady = true;
    if (m_pacer)
    {
      m_pacer->EndFrame(now, m_lastCpuTime + m_lastGpuTime);
    }
    if (m_frameLog)
    {
      // What the frame pacer is told about the frame
      m_frameLog->SetLogValue(m_logColumns.EndFrame, now);
      m_frameLog->SetLogValue(m_logColumns.WorkCpu, m_lastCpuTime);
      m_frameLog->SetLogValue(m_logColumns.WorkGpu, m_lastGpuTime);
      if (m_lastGpuTime.Ticks() > 0 && m_gpuTimeLogFrameIndex.has_value())
      {
        // The GPU time is the one of an earlier frame, so the row says which
        m_frameLog->SetLogUInt64(m_logColumns.WorkGpuFrameIndex, m_gpuTimeLogFrameIndex.value());
      }
    }
  }


  void FramePacingShared::WaitForPresentOnVSync(const uint32_t presentSwapInterval)
  {
    // The window system says when the display refreshes, so the present does not have to be a guess. The frame is aimed at the vertical
    // blank nearest to its start plus its swap interval, and presented during the refresh before the present holds it from. Where in
    // that refresh is a setting, as only a part of it is safe: on the Windows compositor at 240 Hz a present from 45 to 85 % of the
    // refresh was shown at the target, a earlier one two refreshes late and a later one a refresh late.
    const int64_t period = m_vsyncPeriod.Ticks();
    const int64_t aimedTicks = m_frameStartTime.Ticks() + (period * static_cast<int64_t>(m_schedule.SwapInterval));
    const int64_t fromVSync = aimedTicks - m_vsyncTime.Ticks();
    const int64_t refreshes = (fromVSync >= 0 ? (fromVSync + (period / 2)) : (fromVSync - (period / 2))) / period;
    const TickCount target(m_vsyncTime.Ticks() + (refreshes * period));
    const TickCount presentTime(target.Ticks() - (period * static_cast<int64_t>(std::max(presentSwapInterval, 1u))) +
                                ((period * static_cast<int64_t>(m_vsyncPhasePercent)) / 100));

    const TickCount waitStartTime = m_timer.GetTimestamp();
    WaitUntil(presentTime);
    const TickCount waitEndTime = m_timer.GetTimestamp();
    m_lastPresentWait = waitEndTime - waitStartTime;
    // The next frame starts at the vertical blank this one is aimed at, so the frame starts are on the refreshes
    m_nextFrameStartTime = target;
    LogPresentWait(waitStartTime, presentTime, waitEndTime);
    if (m_frameLog)
    {
      m_frameLog->SetLogValue(m_logColumns.HoldTarget, target);
    }
  }


  TickCount FramePacingShared::ToNearestVBlank(const TickCount time) const noexcept
  {
    const int64_t period = m_vsyncPeriod.Ticks();
    const int64_t fromVSync = time.Ticks() - m_vsyncTime.Ticks();
    const int64_t refreshes = (fromVSync >= 0 ? (fromVSync + (period / 2)) : (fromVSync - (period / 2))) / period;
    return TickCount(m_vsyncTime.Ticks() + (refreshes * period));
  }


  void FramePacingShared::WaitForPacerTime(const uint32_t presentSwapInterval, const SamplePacerHold holdMethod)
  {
    // The present holds the frame for its swap interval, but a present of a swapchain need not make the loop wait for the display. So
    // the app waits for the time the frame pacer gives for the start of the next frame, on the side of the present the profile says.
    //
    // The times are kept one frame time apart, counted from the time the frame before was held to and not from where the loop is
    // now: the loop takes time to get from a present to the start of the next frame, and that would be added to every frame. A frame
    // that was more than half a frame time late is where the count starts again, as is the first one.
    //
    // With the vsync of the window system the time is put on its refreshes: the start of a frame on a vertical blank, a present at
    // the place in the refresh the vsync wait presents at. Counted from a time that was on the refreshes the next one is a whole
    // number of them later, so it does not go back and forth between two refreshes.
    const TickCount lastPresentDueTime = m_presentDueTime;
    const TickCount lastFrameStartDueTime = m_frameStartDueTime;
    m_presentDueTime = {};
    m_frameStartDueTime = {};
    const TimeSpan frameTime = m_schedule.NextFrameStartTime - m_frameStartTime;
    if (m_pacerProfile == SamplePacerProfile::Off || frameTime.Ticks() <= 0)
    {
      return;
    }
    const bool onVSync = holdMethod == SamplePacerHold::VSync && m_vsyncTime.Ticks() > 0 && m_vsyncPeriod.Ticks() > 0;
    const int64_t halfFrameTime = frameTime.Ticks() / 2;

    // Once, a while after the first frame that waited: a few refreshes more (SampleConfig::DrainRefreshes). With nothing presented
    // for these refreshes the display shows the presents that queued up while the swapchain was new, and the frames after it are
    // not behind them.
    TimeSpan drainTime;
    if (m_drainPending && m_drainCountStartTime.Ticks() == 0)
    {
      m_drainCountStartTime = m_frameStartTime;
    }
    if (m_drainPending && (m_frameStartTime - m_drainCountStartTime) >= LocalConfig::DrainDelay &&
        (lastPresentDueTime.Ticks() != 0 || lastFrameStartDueTime.Ticks() != 0))
    {
      m_drainPending = false;
      drainTime = TimeSpan((frameTime.Ticks() / static_cast<int64_t>(std::max(m_schedule.SwapInterval, 1u))) * m_drainRefreshes);
      if (m_frameLog && m_drainRefreshes > 0)
      {
        m_frameLog->AddLogEvent("pacerDrain", fmt::format("refreshes={}", m_drainRefreshes));
      }
    }

    if (m_pacerProfile == SamplePacerProfile::RenderLate)
    {
      const bool isOnCount = lastFrameStartDueTime.Ticks() != 0 && (m_frameStartTime - lastFrameStartDueTime).Ticks() <= halfFrameTime;
      TickCount dueTime = (isOnCount ? lastFrameStartDueTime : m_frameStartTime) + frameTime + drainTime;
      if (onVSync)
      {
        dueTime = ToNearestVBlank(dueTime);
      }
      m_nextFrameStartTime = dueTime;
      m_frameStartDueTime = dueTime;
      if (onVSync && m_frameLog)
      {
        m_frameLog->SetLogValue(m_logColumns.HoldTarget, dueTime);
      }
      return;
    }

    const bool isOnCount = lastPresentDueTime.Ticks() != 0 && (m_presentTime - lastPresentDueTime).Ticks() <= halfFrameTime;
    TickCount dueTime = (isOnCount ? lastPresentDueTime : m_frameStartTime) + frameTime + drainTime;
    if (onVSync)
    {
      // The present is made in the refresh before the vertical blank it is aimed at, at the place in it the vsync wait uses
      const TimeSpan fromVBlank((m_vsyncPeriod.Ticks() * static_cast<int64_t>(m_vsyncPhasePercent)) / 100);
      const TickCount refreshStart = ToNearestVBlank(dueTime - fromVBlank);
      dueTime = refreshStart + fromVBlank;
      if (m_frameLog)
      {
        m_frameLog->SetLogValue(m_logColumns.HoldTarget,
                                refreshStart + TimeSpan(m_vsyncPeriod.Ticks() * static_cast<int64_t>(std::max(presentSwapInterval, 1u))));
      }
    }
    const TickCount waitStartTime = m_timer.GetTimestamp();
    WaitUntil(dueTime);
    m_presentTime = m_timer.GetTimestamp();
    m_presentDueTime = dueTime;
    m_lastPresentWait = m_presentTime - waitStartTime;
    LogPresentWait(waitStartTime, dueTime, m_presentTime);
  }


  void FramePacingShared::WaitForPresent(const uint32_t presentSwapInterval)
  {
    m_presentRelativeTarget = {};
    const SamplePacerHold holdMethod = GetHoldMethod();
    m_sampleHeldFrame = m_pacer != nullptr && m_schedule.SwapInterval > presentSwapInterval;
    if (m_frameLog && m_pacer)
    {
      m_frameLog->SetLogInt64(m_logColumns.HoldMethod, ToLogCode(holdMethod));
    }
    if (m_pacer && m_schedule.SwapInterval <= presentSwapInterval)
    {
      // A frame that is held for more refreshes than the present holds it for is handled by the hold methods below
      WaitForPacerTime(presentSwapInterval, holdMethod);
    }
    else
    {
      m_presentDueTime = {};
      m_frameStartDueTime = {};
    }
    if (m_pacer && holdMethod == SamplePacerHold::VSync)
    {
      if (m_schedule.SwapInterval > presentSwapInterval)
      {
        WaitForPresentOnVSync(presentSwapInterval);
      }
      return;
    }
    if (m_pacer && holdMethod == SamplePacerHold::Schedule && m_pacerConfig.RefreshRateHz > 0.0)
    {
      // The presentation engine holds the frame: the present is given a target time and is done right away. The frame is not shown
      // before the target time has passed since the frame before it was shown, and then at the first refresh. Half a refresh less than
      // the swap interval makes that the refresh the frame pacer aims for, with room on both sides for a clock that is a little off.
      const int64_t refreshPeriodTicks = std::llround(static_cast<double>(TimeSpan::TicksPerSecond) / m_pacerConfig.RefreshRateHz);
      const int64_t swapInterval = std::max(m_schedule.SwapInterval, 1u);
      m_presentRelativeTarget = TimeSpan((refreshPeriodTicks * swapInterval) - (refreshPeriodTicks / 2));
      if (m_schedule.SwapInterval > presentSwapInterval)
      {
        // The present does not hold the loop for the swap interval, so the next frame starts when this one is aimed to be shown
        m_nextFrameStartTime = m_schedule.NextFrameStartTime;
      }
      if (m_frameLog)
      {
        m_frameLog->SetLogValue(m_logColumns.PresentTarget, m_presentRelativeTarget);
      }
      return;
    }
    if (!m_pacer || m_schedule.SwapInterval <= presentSwapInterval)
    {
      return;
    }
    // A present that waits for vsync shows the frame at the first refresh the present allows. So a frame that is held for more
    // refreshes than the present can hold it for is presented as late as it can and still be shown at the refresh the frame pacer
    // aims for (sleep, then present). This is a guess: the pacer has no vsync times, the frame is taken to have started at a refresh.
    // The time to hold to is the start of the frame plus its swap interval. Without present feedback that is the intended display time,
    // with it the intended display time is the refresh the frame reaches through the queue of the swapchain, and unknown at first.
    const TimeSpan refreshPeriod(std::llround(static_cast<double>(TimeSpan::TicksPerSecond) / m_pacerConfig.RefreshRateHz));
    const TimeSpan presentHoldTime(refreshPeriod.Ticks() * static_cast<int64_t>(std::max(presentSwapInterval, 1u)));
    const TimeSpan presentMargin = std::min(LocalConfig::MaxPresentMargin, TimeSpan(refreshPeriod.Ticks() / 8));
    const TickCount waitStartTime = m_timer.GetTimestamp();
    const TickCount waitTargetTime = (m_schedule.NextFrameStartTime - presentHoldTime) + presentMargin;
    WaitUntil(waitTargetTime);
    const TickCount waitEndTime = m_timer.GetTimestamp();
    m_lastPresentWait = waitEndTime - waitStartTime;
    m_nextFrameStartTime = m_schedule.NextFrameStartTime;
    LogPresentWait(waitStartTime, waitTargetTime, waitEndTime);
  }


  void FramePacingShared::LogPresentWait(const TickCount beginTime, const TickCount targetTime, const TickCount endTime)
  {
    if (m_frameLog)
    {
      // A wait is logged with the time it aimed at and the time it woke, so how far it overslept is a value
      m_frameLog->SetLogValue(m_logColumns.PresentWait, endTime - beginTime);
      m_frameLog->SetLogValue(m_logColumns.PresentWaitBegin, beginTime);
      m_frameLog->SetLogValue(m_logColumns.PresentWaitTarget, targetTime);
      m_frameLog->SetLogValue(m_logColumns.PresentWaitEnd, endTime);
    }
  }


  void FramePacingShared::WaitUntil(const TickCount time) const
  {
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
    const double detectedRefreshRateHz = ReadDisplayRefreshRateHz();
    if (detectedRefreshRateHz != m_detectedRefreshRateHz)
    {
      m_detectedRefreshRateHz = detectedRefreshRateHz;
      UpdateRefreshRateUI();
    }

    SamplePacerConfig pacerConfig;
    pacerConfig.RefreshRateHz = GetRefreshRateHz();
    pacerConfig.TargetFps = static_cast<uint32_t>(std::max(m_ui.SliderTargetFps->GetValue(), 0));
    pacerConfig.Adaptive = m_ui.SwitchAdaptive->IsChecked();

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
    // Present feedback needs the display times of the presents, so it is only offered while the app is asked to measure them and only
    // used while the swapchain does
    const bool feedbackAvailable = pacerOn && IsPresentTimingWanted();
    if (m_ui.SwitchPacerFeedback->IsEnabled() != feedbackAvailable)
    {
      m_ui.SwitchPacerFeedback->SetEnabled(feedbackAvailable);
    }
    pacerConfig.PresentFeedback = feedbackAvailable && m_presentFeedbackEnabled && m_ui.SwitchPacerFeedback->IsChecked();
    {    // When the display refreshes according to the window system, read once per frame
      const auto window = m_window.lock();
      const NativeWindowVSyncInfo vsyncInfo = window ? window->TryGetVSyncInfo() : NativeWindowVSyncInfo();
      m_vsyncTime = vsyncInfo.IsValid() ? NanosecondTickCountUtil::ToTickCount(vsyncInfo.VSyncTime) : TickCount();
      m_vsyncPeriod = vsyncInfo.IsValid() ? NanosecondTimeSpanUtil::ToTimeSpan(vsyncInfo.RefreshPeriod) : TimeSpan();

      // Variable refresh: once it was seen it stays seen for this hold selection and this refresh rate
      const SamplePacerHold requestedHold = GetRequestedHold();
      if (requestedHold != m_variableRefreshSeenHold || m_detectedRefreshRateHz != m_variableRefreshSeenRateHz)
      {
        m_variableRefreshSeenHold = requestedHold;
        m_variableRefreshSeenRateHz = m_detectedRefreshRateHz;
        m_variableRefreshSeen = false;
      }
      if (m_swapchainRefresh == SampleSwapchainRefresh::Variable || (window && window->TryGetVariableRefreshInfo().IsSeen()))
      {
        m_variableRefreshSeen = true;
      }
    }
    {
      // How a frame is held only matters while the pacer is on and the sample is the one that holds it (with eglSwapInterval the driver
      // does). A method the system can not do is shown as disabled, like every other control that can not be used. One that is selected
      // and stops being possible stays selected: the sample falls back (GetHoldMethod) and the overlay says what is used.
      const bool holdApplies = pacerOn && m_presentMethod == SamplePresentMethod::WaitThenPresent;
      const bool hasVSyncTime = m_vsyncTime.Ticks() > 0 && m_vsyncPeriod.Ticks() > 0 && !m_variableRefreshSeen;
      const std::array<std::pair<UI::RadioButton*, bool>, 4> radios = {
        std::pair<UI::RadioButton*, bool>(m_ui.RadioHoldAuto.get(), holdApplies),
        std::pair<UI::RadioButton*, bool>(m_ui.RadioHoldVSync.get(), holdApplies && hasVSyncTime),
        std::pair<UI::RadioButton*, bool>(m_ui.RadioHoldWait.get(), holdApplies),
        std::pair<UI::RadioButton*, bool>(m_ui.RadioHoldSchedule.get(), holdApplies && m_presentSchedulingSupported)};
      for (const auto& entry : radios)
      {
        if (entry.first->IsEnabled() != entry.second)
        {
          entry.first->SetEnabled(entry.second);
        }
      }
    }

    if (!pacerOn)
    {
      m_pacer.reset();
      m_nextFrameStartTime = {};
      m_pacerChanges = {};
      m_pacerPresentFrames = {};
    }
    else if (!m_pacer)
    {
      m_pacer = std::make_unique<SamplePacer>(pacerConfig);
      // The frames before this one were not paced, so what is queued below the swapchain is to be shown first
      m_drainPending = true;
      m_drainCountStartTime = {};
      // A new pacer counts its frames from one again, so what is remembered about the frames of the one before must not reach it
      m_pacerPresentFrames = {};
      m_nextFrameStartTime = {};
      m_frameStats.Clear();
      m_pacerChanges = {};
    }
    else if (pacerConfig != m_pacerConfig)
    {
      // The pacer starts again with the new settings (a empty frame window, the swap interval of the target frame rate)
      m_pacer->SetConfig(pacerConfig);
      m_nextFrameStartTime = {};
      m_frameStats.Clear();
      m_pacerChanges = {};
    }
    m_pacerConfig = pacerConfig;
  }


  void FramePacingShared::WaitForFrameStart()
  {
    if (m_frameStarted || !m_pacer)
    {
      return;
    }
    if (m_frameStartWaitTime.Ticks() == 0)
    {
      m_frameStartWaitTime = m_timer.GetTimestamp();
    }
    WaitUntil(m_nextFrameStartTime);
  }


  void FramePacingShared::StartFrame()
  {
    if (m_frameStarted)
    {
      return;
    }
    m_frameStarted = true;
    ++m_frameId;

    // The wait can have been made already (WaitForFrameStart), the frame then waited from there
    const TickCount frameWaitStartTime = m_frameStartWaitTime.Ticks() != 0 ? m_frameStartWaitTime : m_timer.GetTimestamp();
    m_frameStartWaitTime = {};
    // The time the start of the frame is held to (zero: it is not held)
    m_frameStartLogTargetTime = m_pacer ? m_nextFrameStartTime : TickCount();
    if (m_pacer)
    {
      // A frame starts when the previous one is shown. A present that was delayed (WaitForPresent) need not wait for the display
      // (a Vulkan swapchain can have a buffer to spare), so wait for the time the frame pacer aimed the previous frame at.
      WaitUntil(m_nextFrameStartTime);
    }
    m_nextFrameStartTime = {};

    const TickCount frameStartTime = m_timer.GetTimestamp();
    m_frameInterval = m_frameStartTime.Ticks() != 0 ? (frameStartTime - m_frameStartTime) : TimeSpan();
    m_frameStartTime = frameStartTime;
    m_frameWork.BeginFrame(m_frameId, frameStartTime);

    // The frame that just ended was held for the swap interval of its schedule (one refresh without the pacer)
    if (m_frameInterval.Ticks() > 0 && m_pacerConfig.RefreshRateHz > 0.0)
    {
      const TimeSpan refreshPeriod(std::llround(static_cast<double>(TimeSpan::TicksPerSecond) / m_pacerConfig.RefreshRateHz));
      m_frameStats.AddFrame(frameStartTime, m_frameInterval, refreshPeriod, std::max(m_schedule.SwapInterval, 1u));
    }

    const TimeSpan frameworkTime(m_updateTime.CurrentTickCount.Ticks());
    if (m_pacer)
    {
      m_schedule = m_pacer->BeginFrame(frameStartTime);
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

    BurnCpu(TimeSpan::FromMilliseconds(static_cast<int64_t>(m_ui.SliderCpuLoad->GetValue())));
  }


  void FramePacingShared::RegisterLogColumns()
  {
    if (!m_frameLog)
    {
      return;
    }
    IFramePacingFrameLog& rLog = *m_frameLog;
    LogColumns& rColumns = m_logColumns;
    rColumns.PacerOn = rLog.RegisterColumn("pacerOn", FramePacingLogUnit::Flag, "1 if the frame pacer of the sample paced the frame");
    rColumns.SwapInterval =
      rLog.RegisterColumn("swapInterval", FramePacingLogUnit::Count, "The number of display refreshes the frame pacer holds the frame for");
    rColumns.PreferredSwapInterval = rLog.RegisterColumn("preferredSwapInterval", FramePacingLogUnit::Count,
                                                         "The swap interval of the target frame rate, the pacer never runs faster");
    rColumns.Change = rLog.RegisterColumn("pacerChange", FramePacingLogUnit::Code,
                                          "What the frame pacer did to the swap interval at this frame: 0 unchanged, 1 slower, 2 faster");
    rColumns.AnimationStep = rLog.RegisterColumn("animationStepTicks", FramePacingLogUnit::DurationTicks,
                                                 "The step of the frame pacer from the animation time of the frame before");
    rColumns.WindowFrames = rLog.RegisterColumn("pacerWindowFrames", FramePacingLogUnit::Count, "The frames in the frame window of the frame pacer");
    rColumns.WindowLateFrames =
      rLog.RegisterColumn("pacerWindowLateFrames", FramePacingLogUnit::Count, "The frames in the frame window the frame pacer counts as late");
    rColumns.WindowStartsAhead =
      rLog.RegisterColumn("pacerWindowStartsAheadTicks", FramePacingLogUnit::DurationTicks,
                          "How far the frames in the frame window began before the times the frame pacer gave for them, added up over the "
                          "frames that were not late: about zero when the app waits for those times, positive when the loop runs ahead");
    rColumns.WindowAverageWork = rLog.RegisterColumn("pacerWindowAverageWorkTicks", FramePacingLogUnit::DurationTicks,
                                                     "The average work of the frames in the frame window of the frame pacer");
    rColumns.WindowSpan =
      rLog.RegisterColumn("pacerWindowSpanTicks", FramePacingLogUnit::DurationTicks, "The time the frame window of the frame pacer spans");
    rColumns.WindowFull = rLog.RegisterColumn("pacerWindowFull", FramePacingLogUnit::Flag, "1 if the frame window of the frame pacer is full");
    rColumns.FrameWaitStart =
      rLog.RegisterColumn("frameWaitStartTicks", FramePacingLogUnit::Ticks,
                          "When the sample began to wait for the start of the frame (it holds the start to the time the frame "
                          "before was aimed at)");
    rColumns.FrameWaitTarget = rLog.RegisterColumn("frameWaitTargetTicks", FramePacingLogUnit::Ticks,
                                                   "The time the start of the frame was held to (empty: it was not held). The frame started at "
                                                   "frameStartTicks, so the difference is how late the wait woke");
    rColumns.FrameStart = rLog.RegisterColumn("frameStartTicks", FramePacingLogUnit::Ticks,
                                              "When the sample started the frame, which is the start the frame pacer is given");
    rColumns.EndFrame =
      rLog.RegisterColumn("endFrameTicks", FramePacingLogUnit::Ticks, "When the work of the frame was done, which is what the frame pacer is told");
    rColumns.WorkCpu = rLog.RegisterColumn("workCpuTicks", FramePacingLogUnit::DurationTicks, "How long the CPU worked on the frame");
    rColumns.WorkGpu = rLog.RegisterColumn("workGpuTicks", FramePacingLogUnit::DurationTicks,
                                           "The GPU time the frame pacer was told with the frame: the one of the last frame that was measured");
    rColumns.WorkGpuFrameIndex =
      rLog.RegisterColumn("workGpuFrameIndex", FramePacingLogUnit::Id,
                          "The frame (frameIndex) the workGpuTicks of the row was measured on: an earlier frame, as the GPU time of a frame "
                          "is not known when the frame ends");
    rColumns.GpuTime = rLog.RegisterColumn("gpuTimeTicks", FramePacingLogUnit::DurationTicks,
                                           "The GPU time of the frame itself, written when it was measured, a frame or more later (empty: the "
                                           "frame was not measured)");
    rColumns.GpuWorkBegin =
      rLog.RegisterColumn("gpuWorkBeginTicks", FramePacingLogUnit::Ticks, "When the GPU started on the frame, on the clock of the framework");
    rColumns.GpuWorkEnd =
      rLog.RegisterColumn("gpuWorkEndTicks", FramePacingLogUnit::Ticks, "When the GPU finished the frame, on the clock of the framework");
    rColumns.FlushCall = rLog.RegisterColumn("flushCallTicks", FramePacingLogUnit::Ticks,
                                             "When glFlush was called after the last command of the frame: the GPU is asked to work on the "
                                             "frame from here (only with --GLFlush, without it the driver decides when, the swap at the latest)");
    rColumns.PresentWait = rLog.RegisterColumn("presentWaitTicks", FramePacingLogUnit::DurationTicks,
                                               "How long the sample delayed the present of the frame, to hold it for its swap interval");
    rColumns.PresentWaitBegin = rLog.RegisterColumn("presentWaitBeginTicks", FramePacingLogUnit::Ticks,
                                                    "When the sample began to wait with the finished frame before its present");
    rColumns.PresentWaitTarget =
      rLog.RegisterColumn("presentWaitTargetTicks", FramePacingLogUnit::Ticks, "The time the wait before the present aimed at");
    rColumns.PresentWaitEnd =
      rLog.RegisterColumn("presentWaitEndTicks", FramePacingLogUnit::Ticks, "When the wait before the present woke, the present follows");
    rColumns.CpuLoad =
      rLog.RegisterColumn("cpuLoadMs", FramePacingLogUnit::Count, "The CPU load setting: the milliseconds the sample is busy per frame");
    rColumns.GpuLoad = rLog.RegisterColumn("gpuLoadSteps", FramePacingLogUnit::Count,
                                           "The GPU load setting: the steps of the background (for the lace the rounds of detail and the "
                                           "samples per pixel come from it)");
    rColumns.PacerFrameId =
      rLog.RegisterColumn("pacerFrameId", FramePacingLogUnit::Id, "The id the frame pacer gave the frame, present feedback is given with it");
    rColumns.NextFrameStart = rLog.RegisterColumn("nextFrameStartTicks", FramePacingLogUnit::Ticks,
                                                  "The start of the frame plus its swap interval according to the frame pacer: what the "
                                                  "waits of the sample hold to");
    rColumns.FeedbackOn =
      rLog.RegisterColumn("pacerFeedbackOn", FramePacingLogUnit::Flag,
                          "1 if the frame pacer is given the display times of the frames: it counts what the display did, it paces the same");
    rColumns.FeedbackDisplay = rLog.RegisterColumn("feedbackDisplayTicks", FramePacingLogUnit::Ticks,
                                                   "The display time of the frame the frame pacer was given as present feedback");
    rColumns.FeedbackPresent = rLog.RegisterColumn("feedbackPresentTicks", FramePacingLogUnit::Ticks,
                                                   "The present time of the frame the frame pacer was given with its display time");
    rColumns.FeedbackReportedNotShown =
      rLog.RegisterColumn("feedbackNotShown", FramePacingLogUnit::Flag,
                          "1 if the frame was reported to the frame pacer as not shown: the presentation engine was done with its present "
                          "and had no display time for it");
    rColumns.FeedbackUsed = rLog.RegisterColumn("pacerFeedbackUsed", FramePacingLogUnit::Count,
                                                "The display times the frame pacer counted from, counted since the pacer was made");
    rColumns.FeedbackRefused =
      rLog.RegisterColumn("pacerFeedbackRefused", FramePacingLogUnit::Count,
                          "The display times the frame pacer refused, counted since the pacer was made: too old, before the present of "
                          "their frame or not a whole number of refreshes after the one before");
    rColumns.FeedbackNotShown =
      rLog.RegisterColumn("pacerFeedbackNotShown", FramePacingLogUnit::Count,
                          "The frames that were reported to the frame pacer as never shown, counted since the pacer was made");
    rColumns.HoldMethod = rLog.RegisterColumn("holdMethod", FramePacingLogUnit::Code,
                                              "How the frame is held for more than one refresh: 0 the sample sleeps on a timer, 1 it waits on "
                                              "the vsync of the window system, 3 the present has a target time");
    rColumns.HoldTarget = rLog.RegisterColumn("holdTargetTicks", FramePacingLogUnit::Ticks,
                                              "The vertical blank the frame was aimed at when it was held by waiting on the vsync");
    rColumns.PresentTarget = rLog.RegisterColumn("presentTargetTicks", FramePacingLogUnit::DurationTicks,
                                                 "The target time the sample asked for: the frame is not to be shown before this long after "
                                                 "the frame before it was shown");
    rColumns.FeedbackMissing =
      rLog.RegisterColumn("pacerFeedbackMissing", FramePacingLogUnit::Count,
                          "The frames the frame pacer was given nothing about, counted since the pacer was made: feedback for a newer "
                          "frame came first, or the frame got too old");
    rColumns.FeedbackLateRefreshes =
      rLog.RegisterColumn("pacerFeedbackLateRefreshes", FramePacingLogUnit::Count,
                          "The refreshes the display fell behind the swap intervals of the frames by its display times, counted since the "
                          "pacer was made: the count of the display to hold against the late frames of the frame pacer");
    rColumns.AnimationError = rLog.RegisterColumn(
      "animationErrorTicks", FramePacingLogUnit::DurationTicks,
      "The animation error of the frame by the display times the system reports: how far the animation moved from the frame presented "
      "before it, less how long after that frame it was shown. Negative: shown too late. Empty unless both frames have a display time");
    rLog.SetLogFact("sample.presentMethod", m_presentMethod == SamplePresentMethod::WaitThenPresent ? "WaitThenPresent" : "SwapInterval");
    rLog.SetLogFact("sample.pacerProfile", m_pacerProfile == SamplePacerProfile::RenderLate
                                             ? "late"
                                             : (m_pacerProfile == SamplePacerProfile::RenderEarly ? "early" : "off"));
    rLog.SetLogFact("sample.pacerSupported", SamplePacer::IsSupported() ? "1" : "0");
    // What a frame started with is in the row of that frame. A log without this fact has it in the row of the frame before when the
    // frame starts in the update (SamplePresentMethod::SwapInterval).
    rLog.SetLogFact("sample.frameStartRow", "own");
    rLog.SetLogFact("sample.glFlush", m_flushWanted ? "1" : "0");
  }


  void FramePacingShared::LogFrameStart(const TickCount waitStartTime)
  {
    if (!m_frameLog)
    {
      return;
    }
    IFramePacingFrameLog& rLog = *m_frameLog;
    const LogColumns& columns = m_logColumns;
    const bool pacerOn = m_pacer != nullptr;
    const SamplePacerHold requestedHold = GetRequestedHold();
    if (!m_hasLoggedPacerConfig || pacerOn != m_loggedPacerOn || m_pacerConfig != m_loggedPacerConfig || requestedHold != m_loggedHold)
    {
      // The settings the frames from here on are paced with
      m_hasLoggedPacerConfig = true;
      m_loggedPacerOn = pacerOn;
      m_loggedPacerConfig = m_pacerConfig;
      m_loggedHold = requestedHold;
      rLog.AddLogEvent("pacerConfig",
                       fmt::format("on={};refreshRateHz={};targetFps={};adaptive={};presentFeedback={};hold={};vsyncPhasePercent={}", pacerOn ? 1 : 0,
                                   m_pacerConfig.RefreshRateHz, m_pacerConfig.TargetFps, m_pacerConfig.Adaptive ? 1 : 0,
                                   m_pacerConfig.PresentFeedback ? 1 : 0, ToString(requestedHold), m_vsyncPhasePercent));
    }

    {
      // What makes the GPU load: the same --GpuLoad is another amount of work with another scene, so a log that does not say
      // which one it was can not be compared with another
      const RaymarchScene scene = GetBackgroundScene();
      if (!m_hasLoggedBackground || scene != m_loggedBackgroundScene)
      {
        m_hasLoggedBackground = true;
        m_loggedBackgroundScene = scene;
        rLog.AddLogEvent("background", fmt::format("scene={}", ToLogName(scene)));
      }
    }

    if (m_variableRefreshSeen != m_loggedVariableRefreshSeen)
    {
      // From here on the vsync wait is not used (or can be used again), see the holdMethod column for what the frames were held with
      m_loggedVariableRefreshSeen = m_variableRefreshSeen;
      rLog.AddLogEvent("holdVariableRefresh", fmt::format("seen={}", m_variableRefreshSeen ? 1 : 0));
    }

    rLog.SetLogValue(columns.PacerOn, pacerOn);
    rLog.SetLogValue(columns.FrameWaitStart, waitStartTime);
    if (m_frameStartLogTargetTime.Ticks() != 0)
    {
      rLog.SetLogValue(columns.FrameWaitTarget, m_frameStartLogTargetTime);
    }
    rLog.SetLogValue(columns.FrameStart, m_frameStartTime);
    rLog.SetLogInt64(columns.CpuLoad, m_ui.SliderCpuLoad->GetValue());
    rLog.SetLogInt64(columns.GpuLoad, m_ui.SliderGpuLoad->GetValue());
    if (pacerOn)
    {
      rLog.SetLogUInt64(columns.SwapInterval, m_schedule.SwapInterval);
      rLog.SetLogInt64(columns.Change, static_cast<int64_t>(m_schedule.Change));
      rLog.SetLogValue(columns.AnimationStep, m_schedule.AnimationStep);
      // The frame window as it is after the frame before this one was measured
      const SamplePacerStatus status = m_pacer->GetStatus();
      rLog.SetLogUInt64(columns.PreferredSwapInterval, status.PreferredSwapInterval);
      rLog.SetLogUInt64(columns.WindowFrames, status.Frames);
      rLog.SetLogUInt64(columns.WindowLateFrames, status.LateFrames);
      rLog.SetLogValue(columns.WindowStartsAhead, status.StartsAhead);
      rLog.SetLogValue(columns.WindowAverageWork, status.AverageWork);
      rLog.SetLogValue(columns.WindowSpan, status.WindowSpan);
      rLog.SetLogValue(columns.WindowFull, status.WindowFull);
      rLog.SetLogUInt64(columns.PacerFrameId, m_schedule.FrameId);
      rLog.SetLogValue(columns.NextFrameStart, m_schedule.NextFrameStartTime);
      rLog.SetLogValue(columns.FeedbackOn, m_pacerConfig.PresentFeedback);
      if (m_pacerConfig.PresentFeedback)
      {
        // What became of the feedback the frame pacer had when it planned this frame
        const SamplePacerFeedbackState feedbackState = m_pacer->GetFeedbackState();
        rLog.SetLogUInt64(columns.FeedbackUsed, feedbackState.Used);
        rLog.SetLogUInt64(columns.FeedbackRefused, feedbackState.Refused);
        rLog.SetLogUInt64(columns.FeedbackNotShown, feedbackState.NotShown);
        rLog.SetLogUInt64(columns.FeedbackMissing, feedbackState.Missing);
        rLog.SetLogUInt64(columns.FeedbackLateRefreshes, feedbackState.LateRefreshes);
      }
    }
  }


  double FramePacingShared::ReadDisplayRefreshRateHz() const
  {
    const auto window = m_window.lock();
    // This is cheap as the window caches it
    return window ? window->TryGetDisplayInfo().RefreshRateHz() : 0.0;
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
    const double refreshRateHz = GetRefreshRateHz();
    m_ui.SliderRefreshRate->SetValue(static_cast<int32_t>(std::lround(refreshRateHz)));
    SetFormattedContent(*m_ui.LabelRefreshRate, "Refresh rate: {:.2f} Hz ({})", refreshRateHz,
                        m_refreshRateOverrideHz.has_value() ? "command line" : "display");
  }


  void FramePacingShared::UpdateTier()
  {
    if (!m_tierFactsReady)
    {
      return;
    }
    SamplePacingTierFacts facts;
    facts.PresentHasSwapInterval = m_presentMethod == SamplePresentMethod::SwapInterval;
    facts.PresentSwapIntervalMax = m_presentSwapIntervalMax;
    facts.PacerOn = m_pacer != nullptr;
    facts.SampleHeldFrame = m_sampleHeldFrame;
    facts.HoldMethod = GetHoldMethod();
    facts.PresentSchedulingSupported = m_presentSchedulingSupported;
    facts.HasVSyncTime = m_vsyncTime.Ticks() > 0 && m_vsyncPeriod.Ticks() > 0 && !m_variableRefreshSeen;
    facts.ExplicitSync = m_explicitSync;
    // It is set again by the next frame the sample holds itself
    m_sampleHeldFrame = false;

    const SamplePacingTierInfo info = SamplePacingTierClassifier::Classify(facts);
    if (m_tierKnown && info == m_tierInfo)
    {
      return;
    }
    // From here on: only when the tier changed (the pacer was switched, another hold method, the system lost or got a vsync time)
    m_tierKnown = true;
    m_tierInfo = info;

    const int32_t inUse = SamplePacingTierClassifier::ToNumber(info.InUse);
    const int32_t best = SamplePacingTierClassifier::ToNumber(info.Best);
    const std::string_view inUseReason = SamplePacingTierClassifier::ToDisplayString(info.InUseReason);
    const std::string_view bestReason = SamplePacingTierClassifier::ToDisplayString(info.BestReason);
    // Two numbers: what the run uses for pacing now, and the best this system can do
    SetFormattedContent(*m_ui.LabelTierInUse, "In use: tier {} of {}, {}", inUse, SamplePacingTierClassifier::TierCount, inUseReason);
    SetFormattedContent(*m_ui.LabelTierBest, "Best here: tier {} of {}, {}", best, SamplePacingTierClassifier::TierCount, bestReason);

    const std::string_view inUseLogReason = SamplePacingTierClassifier::ToLogString(info.InUseReason);
    const std::string_view bestLogReason = SamplePacingTierClassifier::ToLogString(info.BestReason);
    if (m_frameLog)
    {
      // A event and not a fact: the tier of a run can change, and its first value can be one from before the window system had a
      // vsync time
      m_frameLog->AddLogEvent("tier", fmt::format("inUse={};reason={};best={};bestReason={}", inUse, inUseLogReason, best, bestLogReason));
      FSLLOG3_INFO("FramePacing: tier in use {} ({}), best here {} ({}), vsync source '{}'", inUse, inUseLogReason, best, bestLogReason,
                   m_vsyncSourceName);
    }
    else
    {
      FSLLOG3_VERBOSE("FramePacing: tier in use {} ({}), best here {} ({}), vsync source '{}'", inUse, inUseLogReason, best, bestLogReason,
                      m_vsyncSourceName);
    }
  }


  void FramePacingShared::UpdatePacerStatus()
  {
    // The swap interval, the rate it gives and the frames are shown with the frame pacer off as well: every frame is then held for
    // one refresh and the sample counts the late frames itself
    uint32_t swapInterval = 1;
    uint32_t frames = m_frameStats.FrameCount();
    uint32_t lateFrames = m_frameStats.LateFrameCount();
    if (m_pacer)
    {
      const SamplePacerStatus status = m_pacer->GetStatus();
      swapInterval = std::max(status.SwapInterval, 1u);
      frames = status.Frames;
      lateFrames = status.LateFrames;
    }

    SetFormattedContent(*m_ui.LabelPacedRate, "Paced rate: {:.2f} Hz", m_pacerConfig.RefreshRateHz / static_cast<double>(swapInterval));
    if (m_pacer)
    {
      SetFormattedContent(*m_ui.LabelPacerStatus, "Swap interval {}", swapInterval);
    }
    else
    {
      SetFormattedContent(*m_ui.LabelPacerStatus, "Swap interval {} ({})", swapInterval,
                          SamplePacer::IsSupported() ? "pacer off" : "pacer not supported");
    }
    SetFormattedContent(*m_ui.LabelPacerFrames, "Frame {:.2f} ms, {} of {} late", m_frameInterval.TotalMilliseconds(), lateFrames, frames);
  }


  void FramePacingShared::UpdatePacerStats()
  {
    if (!m_ui.SwitchPacerStats->IsChecked())
    {
      // The overlay is hidden
      return;
    }
    const PacerStatsUIRecord& rStats = m_ui.PacerStats;

    // What the sample measures, with the frame pacer on or off
    const SampleFrameTimes frameTimes = m_frameStats.FrameTimes();
    if (m_frameStats.FrameCount() > 0)
    {
      SetFormattedContent(*rStats.FrameTime, "{:.2f} ms ({:.2f} to {:.2f})", frameTimes.Average.TotalMilliseconds(),
                          frameTimes.Min.TotalMilliseconds(), frameTimes.Max.TotalMilliseconds());
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

    if (!m_pacer)
    {
      // Without the pacer every frame is held for one refresh and the sample counts the late frames
      const uint32_t frames = m_frameStats.FrameCount();
      const uint32_t lateFrames = m_frameStats.LateFrameCount();
      SetFormattedContent(*rStats.SwapInterval, "1 ({})", SamplePacer::IsSupported() ? PacerOffValue : "pacer not supported");
      SetFormattedContent(*rStats.LateFrames, "{} of {} ({:.1f} %)", lateFrames, frames, frames > 0u ? ((100.0 * lateFrames) / frames) : 0.0);
      for (UI::Label* pLabel : {rStats.AverageWork.get(), rStats.IntervalChanges.get(), rStats.LastChange.get(), rStats.FrameWindow.get(),
                                rStats.Feedback.get(), rStats.FeedbackLate.get()})
      {
        pLabel->SetContent(PacerOffValue);
      }
      return;
    }

    // What the frame pacer decides on
    const SamplePacerStatus status = m_pacer->GetStatus();
    SetFormattedContent(*rStats.SwapInterval, "{} (preferred {})", status.SwapInterval, status.PreferredSwapInterval);
    SetFormattedContent(*rStats.LateFrames, "{} of {} ({:.1f} %)", status.LateFrames, status.Frames,
                        status.Frames > 0u ? ((100.0 * status.LateFrames) / status.Frames) : 0.0);
    if (status.Frames > 0u && m_schedule.TargetFrameTime.Ticks() > 0)
    {
      SetFormattedContent(*rStats.AverageWork, "{:.2f} ms, {:.0f} % of the frame time", status.AverageWork.TotalMilliseconds(),
                          (100.0 * static_cast<double>(status.AverageWork.Ticks())) / static_cast<double>(m_schedule.TargetFrameTime.Ticks()));
    }
    else
    {
      rStats.AverageWork->SetContent(UnknownValue);
    }
    SetFormattedContent(*rStats.IntervalChanges, "{} slower, {} faster", m_pacerChanges.SlowerCount, m_pacerChanges.FasterCount);
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
      // Nearly everything refused means a display with a variable refresh rate, or a wrong refresh rate
      const SamplePacerFeedbackState feedbackState = m_pacer->GetFeedbackState();
      SetFormattedContent(*rStats.Feedback, "{} used, {} refused, {} missing", feedbackState.Used, feedbackState.Refused, feedbackState.Missing);
      // The count of the display, to hold against the late frames the pacer counts from the frame starts
      SetFormattedContent(*rStats.FeedbackLate, "{} refreshes", feedbackState.LateRefreshes);
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
      for (UI::Label* pLabel :
           {rStats.DisplayError.get(), rStats.DisplayInterval.get(), rStats.AnimationError.get(), rStats.Latency.get(), rStats.TimedFrames.get(),
            rStats.DisplayRefresh.get()})
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
      const std::string_view explicitSyncText = SamplePacingTierClassifier::ToDisplayString(m_explicitSync);
      rPacerStats.ExplicitSync = addStatsRow(*pacerGrid, pacerRow, "Explicit sync");
      rPacerStats.ExplicitSync->SetContent(StringViewLite(explicitSyncText.data(), explicitSyncText.size()));
    }
    rPacerStats.SwapInterval = addStatsRow(*pacerGrid, pacerRow, "Swap interval");
    rPacerStats.FrameTime = addStatsRow(*pacerGrid, pacerRow, "Frame time");
    rPacerStats.LateFrames = addStatsRow(*pacerGrid, pacerRow, "Late frames");
    rPacerStats.Work = addStatsRow(*pacerGrid, pacerRow, "Work");
    rPacerStats.AverageWork = addStatsRow(*pacerGrid, pacerRow, "Average work");
    rPacerStats.PresentWait = addStatsRow(*pacerGrid, pacerRow, "Present wait");
    rPacerStats.IntervalChanges = addStatsRow(*pacerGrid, pacerRow, "Interval changes");
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
    const auto labelCpu = rUIFactory.CreateLabel("CPU");
    labelCpu->SetFontColor(WorkChartCpuColor);
    const auto labelGpu = rUIFactory.CreateLabel("GPU");
    labelGpu->SetFontColor(WorkChartGpuColor);
    const auto labelFrame = rUIFactory.CreateLabel("Frame");
    labelFrame->SetFontColor(WorkChartFrameColor);
    const auto legend = std::make_shared<UI::StackLayout>(context);
    legend->SetOrientation(UI::LayoutOrientation::Vertical);
    legend->SetAlignmentY(UI::ItemAlignment::Center);
    legend->AddChild(rUIFactory.CreateLabel("Work per frame"));
    legend->AddChild(labelFrame);
    legend->AddChild(labelGpu);
    legend->AddChild(labelCpu);

    const auto grid = std::make_shared<UI::GridLayout>(context);
    grid->SetAlignmentX(UI::ItemAlignment::Stretch);
    grid->SetMargin(DpThicknessF::Create(8, 0, 8, 0));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
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
    legend->AddChild(labelError);
    legend->AddChild(rUIFactory.CreateLabel("up: shown too soon"));
    legend->AddChild(rUIFactory.CreateLabel("down: shown too late"));
    legend->AddChild(rUIFactory.CreateLabel("by the display times"));
    legend->AddChild(rUIFactory.CreateLabel("the system reports"));

    const auto grid = std::make_shared<UI::GridLayout>(context);
    grid->SetAlignmentX(UI::ItemAlignment::Stretch);
    grid->SetMargin(DpThicknessF::Create(8, 0, 8, 0));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
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
    const double refreshRateHz = GetRefreshRateHz();
    const TimeSpan refreshPeriod(refreshRateHz > 0.0 ? std::llround(static_cast<double>(TimeSpan::TicksPerSecond) / refreshRateHz) : 0);
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
      if (record.IsJudged && m_frameLog)
      {
        // In the row of the frame it is the error of
        const LogPresentFrame& presentFrame = m_logPresentFrames[record.PresentId % m_logPresentFrames.size()];
        if (presentFrame.PresentId == record.PresentId)
        {
          m_frameLog->SetLogValueAt(presentFrame.FrameIndex, m_logColumns.AnimationError, record.Error);
        }
      }
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
      m_markerStep.CpuStartStep.reset();
      if (m_markerStep.HasMarker)
      {
        m_markerStep.AnimationStep = info.AnimationTime - m_markerStep.AnimationTime;
        if (info.CpuStartTime.has_value() && m_markerStep.CpuStartTime.has_value())
        {
          m_markerStep.CpuStartStep = info.CpuStartTime.value() - m_markerStep.CpuStartTime.value();
        }
      }
      m_markerStep.HasMarker = true;
      m_markerStep.FrameIndex = info.FrameIndex;
      m_markerStep.AnimationTime = info.AnimationTime;
      m_markerStep.CpuStartTime = info.CpuStartTime;
    }

    rStats.Kind->SetContent(ToString(info.Kind));
    SetFormattedContent(*rStats.FrameIndex, "{}", info.FrameIndex);
    // The times are followed by their step from the marker before
    if (m_markerStep.AnimationStep.has_value())
    {
      SetFormattedContent(*rStats.AnimationTime, "{:.3f} ms ({:+.3f})", info.AnimationTime.TotalMilliseconds(),
                          m_markerStep.AnimationStep->TotalMilliseconds());
    }
    else
    {
      SetFormattedContent(*rStats.AnimationTime, "{:.3f} ms", info.AnimationTime.TotalMilliseconds());
    }
    SetFormattedContent(*rStats.RunId, "{}", info.RunId);
    if (info.IntendedDisplayTime.has_value())
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
      SetFormattedContent(*rStats.CpuStartTime, "{:.3f} ms ({:+.3f})", info.CpuStartTime->TotalMilliseconds(),
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
