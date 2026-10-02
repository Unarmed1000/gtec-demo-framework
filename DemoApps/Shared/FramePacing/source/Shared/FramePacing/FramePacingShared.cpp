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
    stackLayout->AddChild(lblHint);
    stackLayout->AddChild(lblHintTimed);
    stackLayout->AddChild(lblHintPacer);

    const auto mainLayout = std::make_shared<UI::GridLayout>(uiFactory->GetContext());
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    mainLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    // The panel with every value of the last marker can be hidden with --HideMarkerStats
    if (!options->IsMarkerStatsHidden())
    {
      mainLayout->AddChild(CreateMarkerStatsWindow(*uiFactory), 0, 0);
    }
    const auto rightBar = uiFactory->CreateRightBar(stackLayout);
    mainLayout->AddChild(rightBar, 1, 0);
    mainLayout->SetLimitToAvailableSpace(true);
    m_uiExtension->SetMainWindow(mainLayout);

    UpdateUI();
    UpdateMarkerStats();
    UpdateRefreshRateUI();
    UpdatePacerStatus();
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
    if (!m_pacer)
    {
      return;
    }
    const TickCount now = m_timer.GetTimestamp();
    const TimeSpan cpuTime = now - m_frameStartTime;
    m_pacer->EndFrame(now, cpuTime + TimeSpan(std::max(gpuTime.Ticks(), int64_t{0})));
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
    WaitUntil((m_schedule.IntendedDisplayTime - presentHoldTime) + presentMargin);
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
    if (!SamplePacer::IsSupported() || !m_ui.SwitchPacer->IsChecked())
    {
      m_pacer.reset();
      m_nextFrameStartTime = {};
    }
    else if (!m_pacer || pacerConfig != m_pacerConfig)
    {
      // The settings of a pacer are fixed, so new settings need a new pacer (it starts with the first frame again)
      m_pacer = std::make_unique<SamplePacer>(pacerConfig);
      m_nextFrameStartTime = {};
      m_frameStats.Clear();
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

    const TimeSpan frameworkTime(m_updateTime.CurrentTickCount.Ticks());
    if (m_pacer)
    {
      m_schedule = m_pacer->BeginFrame(frameStartTime);
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
      // Without the pacer every frame is held for one refresh, and the sample counts the late frames
      if (m_frameInterval.Ticks() > 0 && m_pacerConfig.RefreshRateHz > 0.0)
      {
        const TimeSpan refreshPeriod(std::llround(static_cast<double>(TimeSpan::TicksPerSecond) / m_pacerConfig.RefreshRateHz));
        m_frameStats.AddFrame(frameStartTime, m_frameInterval, refreshPeriod, 1);
      }
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


  std::shared_ptr<UI::BaseWindow> FramePacingShared::CreateMarkerStatsWindow(UI::Theme::IThemeControlFactory& rUIFactory)
  {
    // A panel with every value the last drawn marker carried (top right, next to the right bar and away from the markers on the left side)
    const auto statsGrid = std::make_shared<UI::GridLayout>(rUIFactory.GetContext());
    statsGrid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    statsGrid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Fixed, 360.0f));
    uint32_t statsRow = 0;
    const auto addStatsRow = [&rUIFactory, &statsGrid, &statsRow](const char* const pszName)
    {
      statsGrid->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Auto));
      const auto nameLabel = rUIFactory.CreateLabel(pszName);
      nameLabel->SetMargin(DpThicknessF::Create(0, 0, 16, 0));
      auto valueLabel = rUIFactory.CreateLabel(UnknownValue);
      statsGrid->AddChild(nameLabel, 0, statsRow);
      statsGrid->AddChild(valueLabel, 1, statsRow);
      ++statsRow;
      return valueLabel;
    };
    MarkerStatsUIRecord& rStats = m_ui.MarkerStats;
    rStats.Kind = addStatsRow("Marker");
    rStats.FrameIndex = addStatsRow("Frame index");
    rStats.AnimationTime = addStatsRow("Animation time");
    rStats.RunId = addStatsRow("Run id");
    rStats.IntendedDisplayTime = addStatsRow("Intended display time");
    rStats.TargetFrameTime = addStatsRow("Target frame time");
    rStats.CpuStartTime = addStatsRow("CPU start time");
    rStats.CpuBusyTime = addStatsRow("CPU busy");
    rStats.PreferredFrameTime = addStatsRow("Preferred frame time");
    rStats.Static = addStatsRow("Static");
    rStats.RunStartTime = addStatsRow("Run start time");
    rStats.RunSequenceId = addStatsRow("Sequence id");
    rStats.SyncMarker = addStatsRow("Sync marker");

    const auto statsStack = std::make_shared<UI::StackLayout>(rUIFactory.GetContext());
    statsStack->SetOrientation(UI::LayoutOrientation::Vertical);
    statsStack->AddChild(rUIFactory.CreateLabel("Last marker"));
    statsStack->AddChild(rUIFactory.CreateDivider(UI::LayoutOrientation::Horizontal));
    statsStack->AddChild(statsGrid);
    auto statsWindow = rUIFactory.CreateBackgroundWindow(UI::Theme::WindowType::Transparent, statsStack);
    statsWindow->SetAlignmentX(UI::ItemAlignment::Far);
    statsWindow->SetAlignmentY(UI::ItemAlignment::Near);
    return statsWindow;
  }


  void FramePacingShared::UpdateMarkerStats()
  {
    const MarkerStatsUIRecord& rStats = m_ui.MarkerStats;
    if (!rStats.Kind)
    {
      // The panel is hidden (--HideMarkerStats)
      return;
    }
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
