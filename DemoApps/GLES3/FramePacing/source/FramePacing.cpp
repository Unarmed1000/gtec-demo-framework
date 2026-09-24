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

#include "FramePacing.hpp"
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Math/ConstrainedValue.hpp>
#include <FslBase/Math/Pixel/PxRectangle.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Span/SpanUtil_Array.hpp>
#include <FslDemoApp/Base/FrameInfo.hpp>
#include <FslDemoService/FramePacing/IFramePacingService.hpp>
#include <FslDemoService/Graphics/IGraphicsService.hpp>
#include <FslGraphics/Bitmap/ReadOnlyRawBitmap.hpp>
#include <FslGraphics/Colors.hpp>
#include <FslGraphics/Render/Adapter/INativeBatch2D.hpp>
#include <FslSimpleUI/App/Theme/ThemeSelector.hpp>
#include <FslSimpleUI/Base/Control/Background.hpp>
#include <FslSimpleUI/Base/Control/Image.hpp>
#include <FslSimpleUI/Base/Event/WindowSelectEvent.hpp>
#include <FslSimpleUI/Base/Layout/GridLayout.hpp>
#include <FslSimpleUI/Base/Layout/StackLayout.hpp>
#include <FslSimpleUI/Theme/Base/IThemeControlFactory.hpp>
#include <GLES3/gl3.h>
#include <fmt/format.h>
#include <array>
#include <cmath>
#include <string>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr IO::PathView MenuAtlas("UIAtlas/UIAtlas_160dpi");
      constexpr const char* const RunName = "GLES3.FramePacing";
      //! The bar sweeps across the screen in this time
      constexpr double SweepSeconds = 2.0;
      constexpr int32_t BarWidthPx = 16;
      constexpr int32_t BoxSizePx = 96;
      //! The duration of a timed run in seconds
      constexpr ConstrainedValue<int32_t> TimedRunSeconds(10, 1, 120);
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
  }


  FramePacing::FramePacing(const DemoAppConfig& config)
    : DemoAppGLES3(config)
    , m_uiEventListener(this)
    , m_uiExtension(std::make_shared<UIDemoAppExtension>(config, m_uiEventListener.GetListener(), LocalConfig::MenuAtlas))
    , m_framePacing(config.DemoServiceProvider.TryGet<IFramePacingService>())
  {
    // Give the UI a chance to intercept the various DemoApp events.
    RegisterExtension(m_uiExtension);

    // The service is only available on platforms that support the marker library
    if (m_framePacing)
    {
      // This sample always shows the marker, other apps enable it with --FramePacing
      m_framePacing->SetEnabled(true);
    }

    auto graphicsService = config.DemoServiceProvider.Get<IGraphicsService>();
    m_nativeBatch = graphicsService->GetNativeBatch2D();
    {
      constexpr std::array<uint8_t, 4> WhitePixel = {0xFF, 0xFF, 0xFF, 0xFF};
      const auto rawBitmap =
        ReadOnlyRawBitmap::Create(SpanUtil::AsReadOnlySpan(WhitePixel), PxSize2D::Create(1, 1), PixelFormat::R8G8B8A8_UNORM, BitmapOrigin::UpperLeft);
      m_fillTexture.Reset(graphicsService->GetNativeGraphics(), rawBitmap, Texture2DFilterHint::Nearest);
    }

    // Create a small status panel on the right side (the marker is drawn on the left side)
    auto uiFactory = UI::Theme::ThemeSelector::CreateControlFactory(*m_uiExtension);
    m_ui.LabelStatus = uiFactory->CreateLabel(m_framePacing ? "Frame pacing marker: enabled" : "Frame pacing is not supported on this platform");
    m_ui.LabelRun = uiFactory->CreateLabel("");
    m_ui.ButtonRun = uiFactory->CreateTextButton(UI::Theme::ButtonType::Contained, "Start run");
    m_ui.ButtonRun->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.ButtonRun->SetEnabled(m_framePacing != nullptr);
    auto lblDuration = uiFactory->CreateLabel("Timed run duration (seconds)");
    m_ui.SliderDuration = uiFactory->CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, LocalConfig::TimedRunSeconds);
    m_ui.SliderDuration->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.ButtonTimedRun = uiFactory->CreateTextButton(UI::Theme::ButtonType::Contained, "Start timed run");
    m_ui.ButtonTimedRun->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.ButtonTimedRun->SetEnabled(m_framePacing != nullptr);
    auto lblHint = uiFactory->CreateLabel("Space: start/end a run");
    auto lblHintTimed = uiFactory->CreateLabel("T: start a timed run");

    auto stackLayout = std::make_shared<UI::StackLayout>(uiFactory->GetContext());
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
    stackLayout->AddChild(lblHint);
    stackLayout->AddChild(lblHintTimed);

    auto mainLayout = std::make_shared<UI::GridLayout>(uiFactory->GetContext());
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    mainLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    auto rightBar = uiFactory->CreateRightBar(stackLayout);
    mainLayout->AddChild(rightBar, 1, 0);
    mainLayout->SetLimitToAvailableSpace(true);
    m_uiExtension->SetMainWindow(mainLayout);

    UpdateUI();
  }


  FramePacing::~FramePacing() = default;


  void FramePacing::OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent)
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


  void FramePacing::OnKeyEvent(const KeyEvent& event)
  {
    base_type::OnKeyEvent(event);
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
    default:
      break;
    }
  }


  void FramePacing::Update(const DemoTime& /*demoTime*/)
  {
    if (m_framePacing)
    {
      const int64_t measuredTenths = m_framePacing->GetRunMeasuredTime().Ticks() / (TimeSpan::TicksPerMillisecond * 100);
      if (m_framePacing->GetRunState() != m_cachedRunState || m_framePacing->GetRunId() != m_cachedRunId || measuredTenths != m_cachedMeasuredTenths)
      {
        UpdateUI();
      }
    }
  }


  void FramePacing::Draw(const FrameInfo& frameInfo)
  {
    // Use the exact time the frame is animated for, this is what the marker reports
    m_animationSeconds = frameInfo.Time.CurrentTickCount.TotalSeconds();

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    DrawAnimation();

    m_uiExtension->Draw();
  }


  void FramePacing::ToggleRun()
  {
    if (!m_framePacing)
    {
      return;
    }
    if (m_framePacing->GetRunState() == FramePacingRunState::Idle)
    {
      m_framePacing->BeginRun(LocalConfig::RunName, TimeSpan());
    }
    else
    {
      m_framePacing->EndRun();
    }
    UpdateUI();
  }


  void FramePacing::StartTimedRun()
  {
    if (!m_framePacing || m_framePacing->GetRunState() != FramePacingRunState::Idle)
    {
      return;
    }
    const int32_t seconds = m_ui.SliderDuration->GetValue();
    const std::string runName = fmt::format("{} {}s", LocalConfig::RunName, seconds);
    m_framePacing->BeginRun(StringViewLite(runName), TimeSpan::FromSeconds(seconds));
    UpdateUI();
  }


  void FramePacing::UpdateUI()
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


  void FramePacing::DrawAnimation()
  {
    const PxSize2D windowSizePx = GetWindowSizePx();
    if (!m_nativeBatch || windowSizePx.RawWidth() <= 0 || windowSizePx.RawHeight() <= 0)
    {
      return;
    }
    const auto width = static_cast<double>(windowSizePx.RawWidth());
    const auto height = windowSizePx.RawHeight();

    // Constant speed motion: any hitch or uneven frame pacing is easy to spot
    const double sweep = std::fmod(m_animationSeconds / LocalConfig::SweepSeconds, 1.0);
    const auto barX = static_cast<int32_t>(std::lround(sweep * (width - LocalConfig::BarWidthPx)));

    // A box that moves back and forth in the middle of the screen
    const double pingPong = 1.0 - std::abs((2.0 * std::fmod(m_animationSeconds / (LocalConfig::SweepSeconds * 2.0), 1.0)) - 1.0);
    const auto boxX = static_cast<int32_t>(std::lround(pingPong * (width - LocalConfig::BoxSizePx)));
    const int32_t boxY = (height - LocalConfig::BoxSizePx) / 2;

    m_nativeBatch->Begin(BlendState::Opaque);
    m_nativeBatch->Draw(m_fillTexture, PxRectangle::Create(barX, 0, LocalConfig::BarWidthPx, height), Colors::White());
    m_nativeBatch->Draw(m_fillTexture, PxRectangle::Create(boxX, boxY, LocalConfig::BoxSizePx, LocalConfig::BoxSizePx), Colors::Orange());
    m_nativeBatch->End();
  }
}
