#ifndef SHARED_FRAMEPACING_FRAMEPACINGSHARED_HPP
#define SHARED_FRAMEPACING_FRAMEPACINGSHARED_HPP
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

#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslDemoApp/Base/DemoAppConfig.hpp>
#include <FslDemoApp/Base/DemoTime.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingRunState.hpp>
#include <FslGraphics/Color.hpp>
#include <FslGraphics/Render/Texture2D.hpp>
#include <FslSimpleUI/App/UIDemoAppExtension.hpp>
#include <FslSimpleUI/Base/Control/BackgroundLabelButton.hpp>
#include <FslSimpleUI/Base/Control/Label.hpp>
#include <FslSimpleUI/Base/Control/SliderAndFmtValueLabel.hpp>
#include <fmt/format.h>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

namespace Fsl
{
  class IFramePacingMarkerService;
  namespace UI
  {
    class BaseWindow;
    namespace Theme
    {
      class IThemeControlFactory;
    }
  }
  class INativeBatch2D;
  class KeyEvent;

  //! Shows how an app can control the mb-framepacing frame marker through the IFramePacingMarkerService.
  //! The marker itself is drawn by the host on top of every frame, so apps only need to enable it (or use --FramePacing).
  //! All rendering goes through the API independent INativeBatch2D so the same code is used by the GLES2, GLES3 and Vulkan samples.
  class FramePacingShared final : public UI::EventListener
  {
    //! One value label per value the frame pacing marker carries
    struct MarkerStatsUIRecord
    {
      std::shared_ptr<UI::Label> Kind;
      std::shared_ptr<UI::Label> FrameIndex;
      std::shared_ptr<UI::Label> AnimationTime;
      std::shared_ptr<UI::Label> RunId;
      std::shared_ptr<UI::Label> IntendedDisplayTime;
      std::shared_ptr<UI::Label> TargetFrameTime;
      std::shared_ptr<UI::Label> CpuStartTime;
      std::shared_ptr<UI::Label> CpuBusyTime;
      std::shared_ptr<UI::Label> PreferredFrameTime;
      std::shared_ptr<UI::Label> Static;
      std::shared_ptr<UI::Label> RunStartTime;
      std::shared_ptr<UI::Label> RunSequenceId;
      std::shared_ptr<UI::Label> SyncMarker;
    };

    struct UIRecord
    {
      std::shared_ptr<UI::Label> LabelStatus;
      std::shared_ptr<UI::Label> LabelRun;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonRun;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonTimedRun;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderDuration;
      MarkerStatsUIRecord MarkerStats;
    };

    UI::CallbackEventListenerScope m_uiEventListener;
    std::shared_ptr<UIDemoAppExtension> m_uiExtension;
    std::shared_ptr<IFramePacingMarkerService> m_framePacing;
    std::shared_ptr<INativeBatch2D> m_nativeBatch;
    Texture2D m_fillTexture;
    std::string m_runName;
    UIRecord m_ui;
    PxSize2D m_windowSizePx;
    FramePacingRunState m_cachedRunState{FramePacingRunState::Idle};
    uint32_t m_cachedRunId{0};
    //! The measured time shown in the UI in 1/10 seconds
    int64_t m_cachedMeasuredTenths{-1};
    //! Reused for every per-frame text so updating the marker stats does not allocate
    fmt::memory_buffer m_formatBuffer;

  public:
    //! The color the app should clear the screen with
    static constexpr Color ClearColor = Color(0xFF333333);

    //! @param runName the base name of the runs this sample starts (normally the app name)
    FramePacingShared(const DemoAppConfig& config, std::string runName);
    ~FramePacingShared() final;

    [[nodiscard]] std::shared_ptr<UIDemoAppExtension> GetUIDemoAppExtension() const
    {
      return m_uiExtension;
    }

    // From EventListener
    void OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent) final;

    // Called from the parent app
    void OnKeyEvent(const KeyEvent& event);
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics);
    void Update();
    //! @param drawTime the time the frame is animated for (this is exactly what the marker reports)
    void Draw(const DemoTime& drawTime);

  private:
    void ToggleRun();
    void StartTimedRun();
    void UpdateUI();
    //! Create the panel with every value of the last marker (fills in m_ui.MarkerStats)
    std::shared_ptr<UI::BaseWindow> CreateMarkerStatsWindow(UI::Theme::IThemeControlFactory& rUIFactory);
    void UpdateMarkerStats();

    //! Format into the reused buffer and set it as the label content (the label only copies it if the text changed)
    template <typename... TArgs>
    void SetFormattedContent(UI::Label& rLabel, fmt::format_string<TArgs...> format, TArgs&&... args)
    {
      m_formatBuffer.clear();
      fmt::format_to(std::back_inserter(m_formatBuffer), format, std::forward<TArgs>(args)...);
      rLabel.SetContent(StringViewLite(m_formatBuffer.data(), m_formatBuffer.size()));
    }
    void DrawAnimation(const double animationSeconds);
  };
}

#endif
