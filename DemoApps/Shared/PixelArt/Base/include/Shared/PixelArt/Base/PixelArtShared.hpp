#ifndef SHARED_PIXELART_BASE_PIXELARTSHARED_HPP
#define SHARED_PIXELART_BASE_PIXELARTSHARED_HPP
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
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoApp/Base/DemoAppConfig.hpp>
#include <FslDemoApp/Base/DemoTime.hpp>
#include <FslSimpleUI/App/UIDemoAppExtension.hpp>
#include <FslSimpleUI/Base/Control/BackgroundLabelButton.hpp>
#include <FslSimpleUI/Base/Control/Label.hpp>
#include <FslSimpleUI/Base/Control/SliderAndFmtValueLabel.hpp>
#include <FslSimpleUI/Base/Control/Switch.hpp>
#include <FslSimpleUI/Base/Layout/StackLayout.hpp>
#include <Shared/PixelArt/Base/PixelArtFrameState.hpp>
#include <Shared/PixelArt/Base/PixelArtParamSet.hpp>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Fsl
{
  class IContentManager;
  class IPixelArtSceneRenderer;
  class KeyEvent;
  class MouseButtonEvent;
  class MouseMoveEvent;
  struct PixelArtSceneDesc;

  namespace UI
  {
    class BaseWindow;
    class ChartData;
    namespace Theme
    {
      class IThemeControlFactory;
    }
  }

  //! Everything of the PixelArt apps that does not depend on the graphics API: the list of scenes and the switcher, the UI the adjustable
  //! constants of a scene are shown in (made from the "Params" of its Scene.json), the time and the mouse. It tells the renderer which scene
  //! to load and gives it the values of the shadertoy uniforms every frame (GetFrameState).
  class PixelArtShared final : public UI::EventListener
  {
    struct ParamUIRecord
    {
      uint32_t Index{0};
      std::shared_ptr<UI::SliderAndFmtValueLabel<float>> Slider;
    };

    struct GroupUIRecord
    {
      std::shared_ptr<UI::Switch> Header;
      std::shared_ptr<UI::StackLayout> Content;
    };

    struct UIRecord
    {
      std::shared_ptr<UI::BaseWindow> RightBar;
      std::shared_ptr<UI::BaseWindow> ShowUIBar;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonPrevScene;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonNextScene;
      std::shared_ptr<UI::Label> LabelSceneName;
      std::shared_ptr<UI::Label> LabelSceneNumber;
      //! Why the scene could not be loaded, one label per line
      std::shared_ptr<UI::StackLayout> ErrorLayout;
      //! The params of the scene, rebuilt when the scene changes
      std::shared_ptr<UI::StackLayout> ParamLayout;
      std::vector<ParamUIRecord> Params;
      std::vector<GroupUIRecord> Groups;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonReset;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonReload;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonRestart;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonHideUI;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonShowUI;
      std::shared_ptr<UI::SliderAndFmtValueLabel<float>> SliderRenderScale;
      std::shared_ptr<UI::SliderAndFmtValueLabel<float>> SliderTimeSpeed;
      std::shared_ptr<UI::Switch> SwitchPause;
      //! The chart of the GPU time per frame at the bottom of the scene, shown while its switch is on and the UI is visible
      std::shared_ptr<UI::Switch> SwitchGpuChart;
      std::shared_ptr<UI::BaseWindow> GpuChartBar;
      std::shared_ptr<UI::Label> LabelGpuChartStatus;
    };

    struct SceneRecord
    {
      //! The name of the scene folder
      std::string Id;
      //! The name the UI shows (the folder name until the scene was loaded)
      std::string Name;
      //! Why the scene could not be loaded (empty if it was)
      std::string Error;
      bool Loaded{false};
      //! The values of its params, they are kept while the app runs so a scene looks the same when it is shown again
      PixelArtParamSet Params;
    };

    struct MouseRecord
    {
      bool IsDragging{false};
      //! iMouse in the pixels of the window with the origin in the bottom left corner
      std::array<float, 4> Value{};
    };

    UI::CallbackEventListenerScope m_uiEventListener;
    std::shared_ptr<UIDemoAppExtension> m_uiExtension;
    std::shared_ptr<UI::Theme::IThemeControlFactory> m_uiFactory;
    std::shared_ptr<IContentManager> m_contentManager;
    IPixelArtSceneRenderer& m_renderer;
    UIRecord m_ui;
    std::vector<SceneRecord> m_scenes;
    //! The scene the UI shows
    uint32_t m_selectedScene{0};
    //! The scene the renderer draws (empty if no scene could be loaded)
    std::optional<uint32_t> m_activeScene;
    PxSize2D m_windowSizePx;
    MouseRecord m_mouse;
    double m_time{0.0};
    int32_t m_frame{0};
    PixelArtFrameState m_frameState;
    //! The GPU time of every measured frame in microseconds
    std::shared_ptr<UI::ChartData> m_gpuChartData;
    bool m_isGpuTimerSupported{false};

  public:
    //! @param srgbFramebuffer true if the app draws to a sRGB framebuffer (the UI then draws with linear colors)
    PixelArtShared(const DemoAppConfig& config, IPixelArtSceneRenderer& rRenderer, const bool srgbFramebuffer);
    ~PixelArtShared() final;

    [[nodiscard]] std::shared_ptr<UIDemoAppExtension> GetUIDemoAppExtension() const
    {
      return m_uiExtension;
    }

    // From EventListener
    void OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent) final;

    // Called from the parent app
    void OnKeyEvent(const KeyEvent& event);
    void OnMouseButtonEvent(const MouseButtonEvent& event);
    void OnMouseMoveEvent(const MouseMoveEvent& event);
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics);
    void Update(const DemoTime& demoTime);
    //! Draw the UI (after the scene)
    void Draw();

    //! True if the renderer has a scene to draw
    [[nodiscard]] bool HasActiveScene() const noexcept
    {
      return m_activeScene.has_value();
    }

    //! Tell the UI if the app can measure the GPU time of a frame (the chart says it is not available if it can not)
    void SetGpuTimerSupported(const bool supported);

    //! Add the GPU time of a frame to the chart (call it once for every measured frame)
    void AddGpuTime(const TimeSpan gpuTime);

    //! The values of the shadertoy uniforms of the current frame (valid after Update)
    [[nodiscard]] const PixelArtFrameState& GetFrameState() const noexcept
    {
      return m_frameState;
    }

  private:
    //! Load the scene and show it. If it can not be loaded its error is shown and the renderer keeps the scene it had.
    void SelectScene(const uint32_t sceneIndex);
    void SelectNextScene(const int32_t direction);
    //! Start the scene from the beginning: iTime and iFrame are zero and the buffers are cleared
    void Restart();
    void ResetParams();
    void SetUIVisible(const bool visible);
    void UpdateGpuChartVisibility();
    std::shared_ptr<UI::BaseWindow> CreateGpuChartBar(UI::Theme::IThemeControlFactory& rFactory);
    void RebuildParamUI();
    void UpdateSceneLabels();
    void ApplyParamUI();
    //! The part of the window the scene is shown in: left of the UI panel, or all of the window when the panel is hidden
    [[nodiscard]] PxSize2D GetSceneSizePx() const;
    //! The size of the buffers: the scene size scaled by the render scale
    [[nodiscard]] PxSize2D GetRenderSizePx(const PxSize2D sceneSizePx) const;
  };
}

#endif
