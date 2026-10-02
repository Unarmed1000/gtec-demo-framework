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
#include <FslDemoService/FramePacingMarker/IFramePacingMarkerService.hpp>
#include <FslDemoService/Graphics/IGraphicsService.hpp>
#include <FslGraphics/Bitmap/ReadOnlyRawBitmap.hpp>
#include <FslGraphics/Colors.hpp>
#include <FslGraphics/Render/Adapter/INativeBatch2D.hpp>
#include <FslNativeWindow/Base/INativeWindow.hpp>
#include <FslNativeWindow/Base/NativeWindowDisplayInfo.hpp>
#include <FslNativeWindow/Base/VirtualKey.hpp>
#include <FslSimpleUI/App/Theme/ThemeSelector.hpp>
#include <FslSimpleUI/Base/Control/Background.hpp>
#include <FslSimpleUI/Base/Control/Image.hpp>
#include <FslSimpleUI/Base/Control/ScrollViewer.hpp>
#include <FslSimpleUI/Base/Event/WindowSelectEvent.hpp>
#include <FslSimpleUI/Base/Layout/GridLayout.hpp>
#include <FslSimpleUI/Base/Layout/StackLayout.hpp>
#include <FslSimpleUI/Theme/Base/IThemeControlFactory.hpp>
#include <FslSimpleUI/Theme/Base/WindowType.hpp>
#include <Shared/FramePacing/FramePacingShared.hpp>
#include <Shared/FramePacing/OptionParser.hpp>
#include <Shared/FramePacing/SampleConfig.hpp>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
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
      //! The raymarched background: the flight through the lattice repeats after this time
      constexpr double TravelSeconds = 40.0;
      //! The raymarched background: the time of one sway of the camera (the waves and the pulses of light run with it)
      constexpr double SwaySeconds = 29.0;
      //! The raymarched background: the time the shape and the colors of the lattice change in
      constexpr double MorphSeconds = 61.0;
      //! The duration of a timed run in seconds
      constexpr ConstrainedValue<int32_t> TimedRunSeconds(10, 1, 120);
      //! WaitForPresent presents this long after the last refresh before the one the frame pacer aims for (at most an eighth of a refresh)
      constexpr TimeSpan MaxPresentMargin(TimeSpan::TicksPerMillisecond);
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

    //! Shown for a value the marker reports as unknown
    constexpr const char* UnknownValue = "unknown";
    //! Shown for a value only the frame pacer has while it is off
    constexpr const char* PacerOffValue = "pacer off";
    //! The width of the name column of the stats panel, so the values of its sections line up
    constexpr float StatsNameColumnWidthDp = 216.0f;
    constexpr float StatsValueColumnWidthDp = 360.0f;

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
  {
    const auto options = config.GetOptions<OptionParser>();
    m_refreshRateOverrideHz = options->GetPacerRefreshRateHz();
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
    m_ui.SliderDuration = uiFactory->CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, LocalConfig::TimedRunSeconds);
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
    m_ui.LabelPacerStatus = uiFactory->CreateLabel("");
    m_ui.LabelPacerFrames = uiFactory->CreateLabel("");
    const auto lblCpuLoad = uiFactory->CreateLabel("CPU load (ms per frame)");
    m_ui.SliderCpuLoad =
      uiFactory->CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, WithValue(SampleConfig::CpuLoadMs, options->GetCpuLoadMs()));
    m_ui.SliderCpuLoad->SetAlignmentX(UI::ItemAlignment::Stretch);
    const auto lblGpuLoad = uiFactory->CreateLabel("GPU load (ray steps, 0 = off)");
    m_ui.SliderGpuLoad =
      uiFactory->CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, WithValue(SampleConfig::GpuLoadSteps, options->GetGpuLoadSteps()));
    m_ui.SliderGpuLoad->SetAlignmentX(UI::ItemAlignment::Stretch);

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
    stackLayout->AddChild(m_ui.LabelRefreshRate);
    stackLayout->AddChild(m_ui.LabelPacedRate);
    stackLayout->AddChild(m_ui.SliderRefreshRate);
    stackLayout->AddChild(lblTargetFps);
    stackLayout->AddChild(m_ui.SliderTargetFps);
    stackLayout->AddChild(m_ui.SwitchAdaptive);
    stackLayout->AddChild(m_ui.LabelPacerStatus);
    stackLayout->AddChild(m_ui.LabelPacerFrames);
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(lblCpuLoad);
    stackLayout->AddChild(m_ui.SliderCpuLoad);
    stackLayout->AddChild(lblGpuLoad);
    stackLayout->AddChild(m_ui.SliderGpuLoad);
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(m_ui.SwitchMarkerStats);
    stackLayout->AddChild(m_ui.SwitchPacerStats);
    stackLayout->AddChild(uiFactory->CreateDivider(UI::LayoutOrientation::Horizontal));
    stackLayout->AddChild(lblHint);
    stackLayout->AddChild(lblHintTimed);
    stackLayout->AddChild(lblHintPacer);

    const auto mainLayout = std::make_shared<UI::GridLayout>(uiFactory->GetContext());
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    mainLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    // The overlays with every value of the last marker and the frame pacing stats, each is shown while its switch is on
    mainLayout->AddChild(CreateStatsWindow(*uiFactory), 0, 0);
    // The controls can be scrolled, as a low window does not have room for all of them
    stackLayout->SetMargin(DpThicknessF::Create(0, 0, 8, 0));
    const auto scrollViewer = uiFactory->CreateScrollViewer(stackLayout, UI::ScrollModeFlags::TranslateY, false);
    const auto rightBar = uiFactory->CreateRightBar(scrollViewer);
    mainLayout->AddChild(rightBar, 1, 0);
    mainLayout->SetLimitToAvailableSpace(true);
    m_uiExtension->SetMainWindow(mainLayout);

    UpdateUI();
    UpdateStatsVisibility();
    UpdateMarkerStats();
    UpdateRefreshRateUI();
    UpdatePacerStatus();
    UpdatePacerStats();
  }


  FramePacingShared::~FramePacingShared() = default;


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
    if (m_presentMethod == SamplePresentMethod::SwapInterval)
    {
      // The frame starts here: the host updates the app right after the swap of the previous frame, which waited for the display
      StartFrame();
    }

    UpdatePacerStatus();
    UpdateStatsVisibility();
    UpdatePacerStats();
    UpdateMarkerStats();
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

    // Use the exact time the frame is animated for, this is what the marker reports
    DrawAnimation(m_animationTime.TotalSeconds());

    m_uiExtension->Draw();
  }


  RaymarchParams FramePacingShared::GetRaymarchParams()
  {
    // The background is animated for the animation time of the frame, so the frame has to be started
    StartFrame();

    const double animationSeconds = m_animationTime.TotalSeconds();
    RaymarchParams params;
    params.Steps = m_ui.SliderGpuLoad->GetValue();
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
    if (m_pacer)
    {
      m_pacer->EndFrame(now, m_lastCpuTime + m_lastGpuTime);
    }
  }


  void FramePacingShared::WaitForPresent(const uint32_t presentSwapInterval)
  {
    if (!m_pacer || m_schedule.SwapInterval <= presentSwapInterval)
    {
      return;
    }
    // A present that waits for vsync shows the frame at the first refresh the present allows. So a frame that is held for more
    // refreshes than the present can hold it for is presented as late as it can and still be shown at the refresh the frame pacer
    // aims for (sleep, then present). This is a guess: the pacer has no vsync times, the frame is taken to have started at a refresh.
    const TimeSpan refreshPeriod(std::llround(static_cast<double>(TimeSpan::TicksPerSecond) / m_pacerConfig.RefreshRateHz));
    const TimeSpan presentHoldTime(refreshPeriod.Ticks() * static_cast<int64_t>(std::max(presentSwapInterval, 1u)));
    const TimeSpan presentMargin = std::min(LocalConfig::MaxPresentMargin, TimeSpan(refreshPeriod.Ticks() / 8));
    const TickCount waitStartTime = m_timer.GetTimestamp();
    WaitUntil((m_schedule.IntendedDisplayTime - presentHoldTime) + presentMargin);
    m_lastPresentWait = m_timer.GetTimestamp() - waitStartTime;
    m_nextFrameStartTime = m_schedule.IntendedDisplayTime;
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

    if (!pacerOn)
    {
      m_pacer.reset();
      m_nextFrameStartTime = {};
      m_pacerChanges = {};
    }
    else if (!m_pacer)
    {
      m_pacer = std::make_unique<SamplePacer>(pacerConfig);
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


  void FramePacingShared::StartFrame()
  {
    if (m_frameStarted)
    {
      return;
    }
    m_frameStarted = true;

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

    BurnCpu(TimeSpan::FromMilliseconds(static_cast<int64_t>(m_ui.SliderCpuLoad->GetValue())));
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

    if (!m_pacer)
    {
      // Without the pacer every frame is held for one refresh and the sample counts the late frames
      const uint32_t frames = m_frameStats.FrameCount();
      const uint32_t lateFrames = m_frameStats.LateFrameCount();
      SetFormattedContent(*rStats.SwapInterval, "1 ({})", SamplePacer::IsSupported() ? PacerOffValue : "pacer not supported");
      SetFormattedContent(*rStats.LateFrames, "{} of {} ({:.1f} %)", lateFrames, frames, frames > 0u ? ((100.0 * lateFrames) / frames) : 0.0);
      for (UI::Label* pLabel : {rStats.AverageWork.get(), rStats.IntervalChanges.get(), rStats.LastChange.get(), rStats.FrameWindow.get()})
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
    // A panel with every value the last drawn marker carried and the frame pacing stats (top right, next to the right bar and away from
    // the markers on the left side). Each section is a grid of names and values, the columns have the same widths so they line up.
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
    rPacerStats.SwapInterval = addStatsRow(*pacerGrid, pacerRow, "Swap interval");
    rPacerStats.FrameTime = addStatsRow(*pacerGrid, pacerRow, "Frame time");
    rPacerStats.LateFrames = addStatsRow(*pacerGrid, pacerRow, "Late frames");
    rPacerStats.Work = addStatsRow(*pacerGrid, pacerRow, "Work");
    rPacerStats.AverageWork = addStatsRow(*pacerGrid, pacerRow, "Average work");
    rPacerStats.PresentWait = addStatsRow(*pacerGrid, pacerRow, "Present wait");
    rPacerStats.IntervalChanges = addStatsRow(*pacerGrid, pacerRow, "Interval changes");
    rPacerStats.LastChange = addStatsRow(*pacerGrid, pacerRow, "Last change");
    rPacerStats.FrameWindow = addStatsRow(*pacerGrid, pacerRow, "Frame window");

    const auto createSection = [&rUIFactory](const char* const pszCaption, const std::shared_ptr<UI::GridLayout>& grid)
    {
      auto section = std::make_shared<UI::StackLayout>(rUIFactory.GetContext());
      section->SetOrientation(UI::LayoutOrientation::Vertical);
      section->AddChild(rUIFactory.CreateLabel(pszCaption));
      section->AddChild(rUIFactory.CreateDivider(UI::LayoutOrientation::Horizontal));
      section->AddChild(grid);
      return section;
    };
    const auto markerSection = createSection("Last marker", markerGrid);
    // Some air between the two sections
    markerSection->SetMargin(DpThicknessF::Create(0, 0, 0, 12));
    const auto pacerSection = createSection("Frame pacing", pacerGrid);

    const auto statsStack = std::make_shared<UI::StackLayout>(rUIFactory.GetContext());
    statsStack->SetOrientation(UI::LayoutOrientation::Vertical);
    statsStack->AddChild(markerSection);
    statsStack->AddChild(pacerSection);
    auto statsWindow = rUIFactory.CreateBackgroundWindow(UI::Theme::WindowType::Transparent, statsStack);
    statsWindow->SetAlignmentX(UI::ItemAlignment::Far);
    statsWindow->SetAlignmentY(UI::ItemAlignment::Near);
    m_ui.MarkerStatsSection = markerSection;
    m_ui.PacerStatsSection = pacerSection;
    m_ui.StatsWindow = statsWindow;
    return statsWindow;
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
    const bool showMarker = m_ui.SwitchMarkerStats->IsChecked();
    const bool showPacer = m_ui.SwitchPacerStats->IsChecked();
    setVisible(*m_ui.MarkerStatsSection, showMarker);
    setVisible(*m_ui.PacerStatsSection, showPacer);
    // The background of the overlays is only there while one of them is shown
    setVisible(*m_ui.StatsWindow, showMarker || showPacer);
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
