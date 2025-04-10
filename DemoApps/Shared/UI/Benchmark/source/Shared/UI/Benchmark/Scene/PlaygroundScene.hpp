#ifndef SHARED_UI_BENCHMARK_SCENE_PLAYGROUNDSCENE_HPP
#define SHARED_UI_BENCHMARK_SCENE_PLAYGROUNDSCENE_HPP
/****************************************************************************************************************************************************
 * Copyright 2021-2022, 2024 NXP
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
 *    * Neither the name of the NXP. nor the names of
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

#include <FslDemoApp/Shared/Host/DemoWindowMetrics.hpp>
#include <FslGraphics/Sprite/ImageSprite.hpp>
#include <FslSimpleUI/Base/Control/Background.hpp>
#include <FslSimpleUI/Base/Control/BackgroundLabelButton.hpp>
#include <FslSimpleUI/Base/Control/FmtValueLabel.hpp>
#include <FslSimpleUI/Base/Control/Image.hpp>
#include <FslSimpleUI/Base/Control/RadioButton.hpp>
#include <FslSimpleUI/Base/Control/SliderAndFmtValueLabel.hpp>
#include <FslSimpleUI/Base/Control/Switch.hpp>
#include <FslSimpleUI/Base/Layout/Layout.hpp>
#include <FslSimpleUI/Base/Transition/TransitionUIColor.hpp>
#include <FslSimpleUI/Controls/Charts/Common/AverageData.hpp>
#include <FslSimpleUI/Controls/Charts/Data/ChartData.hpp>
#include <FslSimpleUI/Controls/Experimental/ResizeableArea.hpp>
#include <FslSimpleUI/Render/Base/RenderOptionFlags.hpp>
#include <FslSimpleUI/Render/Base/RenderSystemInfo.hpp>
#include <Shared/UI/Benchmark/Persistence/AppRenderMethod.hpp>
#include <Shared/UI/Benchmark/Persistence/AppSettings.hpp>
#include <Shared/UI/Benchmark/Persistence/Bench/AppBenchmarkScene.hpp>
#include <Shared/UI/Benchmark/RenderMethodInfo.hpp>
#include <Shared/UI/Benchmark/Scene/Control/RenderOptionControls.hpp>
#include <future>
#include <memory>
#include "Control/CpuDetailedLegendRecord.hpp"
#include "IScene.hpp"

namespace Fsl
{
  struct AppBenchSettings;
  class DemoAppExtensionForwarder;
  struct SceneCreateInfo;
  class TestAppHost;
  class UIDemoAppExtension;

  namespace UI
  {
    class ActivityStack;
    class ColorDialogActivity;
    class FrameAnalysisDialogActivity;
    struct RenderSystemStats;
    class SlidingPanel;

    namespace Theme
    {
      class IThemeControlFactory;
    }
  }

  class PlaygroundScene final : public IScene
  {
    enum class InputState
    {
      Playground,
      SettingsSubActivity,
      FrameAnalysisSubActivity,
      ColorSubActivity,
      BenchConfigSubActivity,
      Closing
    };

    struct StatsOverlayUI
    {
      std::array<std::shared_ptr<UI::FmtValueLabel<uint32_t>>, 8> Entries;
      std::shared_ptr<UI::Background> MainLayout;
    };

    struct UISwitchButtons
    {
      std::shared_ptr<UI::Switch> SwitchStats;
      std::shared_ptr<UI::Switch> SwitchChart;
    };

    struct OptionBarUI
    {
      std::shared_ptr<UI::Layout> Layout;
      UISwitchButtons UI;
      RenderOptionControls RenderOptions;
      // UI options
      std::shared_ptr<UI::Switch> SwitchUseDrawCache;
      std::shared_ptr<UI::Switch> SwitchOnDemand;
      std::shared_ptr<UI::Switch> SwitchSdfFont;
      std::shared_ptr<UI::Switch> SwitchEmulateDpi;
      std::shared_ptr<UI::SliderAndFmtValueLabel<int32_t>> SliderEmulatedDpi;
      std::shared_ptr<UI::ButtonBase> ButtonConfig;
      std::shared_ptr<UI::ButtonBase> ButtonFrameAnalysis;
      std::shared_ptr<UI::ButtonBase> ButtonRecord;
      std::shared_ptr<UI::ButtonBase> ButtonBench;
      std::shared_ptr<UI::Image> ImageIdle;
    };

    struct BottomBarUI
    {
      bool Enabled{false};
      std::shared_ptr<UI::BaseWindow> Main;
      CpuDetailedLegendRecord CpuLegendUI;
      std::shared_ptr<UI::BackgroundLabelButton> ButtonClear;
    };

    struct ProfileUI
    {
      std::shared_ptr<UI::SlidingPanel> BottomSlidingPanel;
      BottomBarUI BottomBar;
      StatsOverlayUI StatsOverlay;
      StatsOverlayUI StatsOverlay2;
      OptionBarUI OptionsBar;
      std::shared_ptr<UI::Layout> MainLayout;
      std::shared_ptr<UI::Layout> AppLayout;
      std::shared_ptr<UI::ActivityStack> ActivityStack;
      std::shared_ptr<UI::BaseWindow> ContentArea;
      std::shared_ptr<UI::ResizeableArea> ResizeableClipArea;
    };

    struct AnimRecord
    {
      UI::TransitionUIColor OverlayColorStatsApp;
      UI::TransitionUIColor OverlayColorStats;

      explicit AnimRecord(const TimeSpan transitionTimeSpan)
        : OverlayColorStatsApp(transitionTimeSpan, TransitionType::Smooth)
        , OverlayColorStats(transitionTimeSpan, TransitionType::Smooth)
      {
      }

      void Update(const TimeSpan& deltaTime)
      {
        OverlayColorStatsApp.Update(deltaTime);
        OverlayColorStats.Update(deltaTime);
      }

      bool IsBusy() const
      {
        return !OverlayColorStatsApp.IsCompleted() || !OverlayColorStats.IsCompleted();
      }
    };
    struct CachedState
    {
      bool WasTestAppUIIdle{false};
      bool WasAppUIIdle{false};
    };

    std::shared_ptr<DemoAppExtensionForwarder> m_forwarder;
    std::shared_ptr<UI::Theme::IThemeControlFactory> m_uiControlFactory;
    DemoWindowMetrics m_windowMetrics;
    std::shared_ptr<const UIDemoAppExtension> m_uiExtension;
    bool m_gpuProfilerSupported{false};

    std::vector<RenderMethodInfo> m_renderRecords;

    std::shared_ptr<UI::ChartData> m_data;
    UI::AverageData m_dataAverage;

    ProfileUI m_uiProfile;
    AnimRecord m_anim;

    std::shared_ptr<TestAppHost> m_testAppHost;

    std::shared_ptr<AppSettings> m_settings;
    CachedState m_cachedState;

    InputState m_inputState{InputState::Playground};

    std::shared_ptr<UI::FrameAnalysisDialogActivity> m_frameAnalysisDialog;
    std::shared_ptr<UI::ColorDialogActivity> m_colorDialog;

    std::shared_ptr<AppTestSettings> m_settingsResult;
    std::shared_ptr<AppBenchSettings> m_benchResult;
    std::future<bool> m_configDialogPromise;

    std::optional<NextSceneRecord> m_nextSceneRecord;

  public:
    explicit PlaygroundScene(const SceneCreateInfo& createInfo);
    ~PlaygroundScene() override;

    // std::shared_ptr<DemoAppExtension> GetDemoAppExtension() const
    //{
    //  return m_forwarder;
    //}


    std::optional<NextSceneRecord> TryGetNextScene() const final
    {
      return m_nextSceneRecord;
    }

    void OnFrameSequenceBegin() final;
    void OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent) final;
    void OnContentChanged(const std::shared_ptr<UI::WindowContentChangedEvent>& theEvent) final;
    void OnKeyEvent(const KeyEvent& event) final;
    void OnConfigurationChanged(const DemoWindowMetrics& windowMetrics) final;
    void Update(const DemoTime& demoTime) final;
    bool Resolve(const DemoTime& demoTime) final;
    void Draw(const DemoTime& demoTime) final;
    void OnDrawSkipped(const FrameInfo& frameInfo) final;
    void OnFrameSequenceEnd() final;

  private:
    void UpdateSettingsSubActivityState();
    void UpdateFrameAnalysisSubActivityState();
    void UpdateColorSubActivityState();
    void UpdateBenchConfigSubActivityState();

    void ShowBenchmarkConfig();
    void ShowColorDialog();
    void ShowFrameAnalysisDialogActivity();
    void ShowSettings();

    void SetDefaultValues();
    void ClearGraph();
    void RestoreUISettings(const RenderMethodInfo& renderMethodInfo);

    uint32_t FrameAnalysisGetCurrentMaxDrawCalls() const;
    void FrameAnalysisSetMaxDrawCalls(const uint32_t maxDrawCalls);
    static void UpdateStats(StatsOverlayUI& overlay, const UI::RenderSystemStats& stats);

    void RestartTestApp(const bool storeSettingsBeforeRestart);

    void ApplyRenderSettings();

    static ProfileUI CreateProfileUI(UI::Theme::IThemeControlFactory& uiFactory, const uint16_t currentDensityDpi,
                                     const std::shared_ptr<UI::ChartData>& data, const AppUISettings& settings);
    static OptionBarUI CreateOptionBarUI(UI::Theme::IThemeControlFactory& uiFactory, const std::shared_ptr<UI::WindowContext>& context,
                                         const uint16_t currentDensityDpi, const AppUISettings& settings);
    static UISwitchButtons CreateUISwitchButtons(UI::Theme::IThemeControlFactory& uiFactory, const std::shared_ptr<UI::WindowContext>& context,
                                                 const AppUISettings& settings);
    static BottomBarUI CreateBottomBar(UI::Theme::IThemeControlFactory& uiFactory, const std::shared_ptr<UI::WindowContext>& context,
                                       const std::shared_ptr<UI::ChartData>& data);
    static StatsOverlayUI CreateStatsOverlayUI(UI::Theme::IThemeControlFactory& uiFactory, const std::shared_ptr<UI::WindowContext>& context);

    void SetDpi(const uint16_t densityDpi);
    void SetUseDrawCache(const bool useDrawCache);

    void BeginClose(const SceneId nextSceneId, std::shared_ptr<ISceneConfig> sceneConfig = {});
  };
}

#endif
