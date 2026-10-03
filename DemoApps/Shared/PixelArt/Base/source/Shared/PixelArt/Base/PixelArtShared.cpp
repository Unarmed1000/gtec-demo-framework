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
#include <FslBase/Math/Dp/DpThicknessF.hpp>
#include <FslDemoApp/Base/Service/Content/IContentManager.hpp>
#include <FslDemoApp/Base/Service/Events/Basic/KeyEvent.hpp>
#include <FslDemoApp/Base/Service/Events/Basic/MouseButtonEvent.hpp>
#include <FslDemoApp/Base/Service/Events/Basic/MouseMoveEvent.hpp>
#include <FslDemoApp/Shared/Host/DemoWindowMetrics.hpp>
#include <FslNativeWindow/Base/VirtualKey.hpp>
#include <FslNativeWindow/Base/VirtualMouseButton.hpp>
#include <FslSimpleUI/App/Theme/ThemeSelector.hpp>
#include <FslSimpleUI/Base/Control/Background.hpp>
#include <FslSimpleUI/Base/Control/Image.hpp>
#include <FslSimpleUI/Base/Control/InputCatcher.hpp>
#include <FslSimpleUI/Base/Control/ScrollViewer.hpp>
#include <FslSimpleUI/Base/DpLayoutSize1D.hpp>
#include <FslSimpleUI/Base/Event/WindowSelectEvent.hpp>
#include <FslSimpleUI/Base/ItemAlignment.hpp>
#include <FslSimpleUI/Base/ItemVisibility.hpp>
#include <FslSimpleUI/Base/Layout/GridLayout.hpp>
#include <FslSimpleUI/Controls/Charts/AreaChart.hpp>
#include <FslSimpleUI/Controls/Charts/Common/ChartGridLinesFps.hpp>
#include <FslSimpleUI/Controls/Charts/Data/ChartData.hpp>
#include <FslSimpleUI/Theme/Base/IThemeControlFactory.hpp>
#include <FslSimpleUI/Theme/Base/IThemeResources.hpp>
#include <Shared/PixelArt/Base/IPixelArtSceneRenderer.hpp>
#include <Shared/PixelArt/Base/PixelArtOptionParser.hpp>
#include <Shared/PixelArt/Base/PixelArtSceneLoader.hpp>
#include <Shared/PixelArt/Base/PixelArtShared.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>
#include <map>
#include <utility>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr IO::PathView MenuAtlas("UIAtlas/UIAtlas_160dpi");
      constexpr float PanelWidthDp = 340.0f;
      //! A error is shown in lines of at most this many characters
      constexpr std::size_t ErrorLineLength = 34;
      constexpr ConstrainedValue<float> RenderScale(1.0f, 0.25f, 1.0f);
      constexpr float RenderScaleStep = 0.05f;
      constexpr ConstrainedValue<float> TimeSpeed(1.0f, 0.0f, 4.0f);
      constexpr float TimeSpeedStep = 0.05f;
      //! The color and the height of the GPU time chart (the orange the FramePacing sample uses for the GPU)
      constexpr UI::UIColor GpuChartColor(PackedColor32(0xFFE0902A));
      constexpr float GpuChartHeightDp = 100.0f;
    }

    IO::Path GetAtlasName(const bool srgbFramebuffer)
    {
      return IO::Path::Combine(srgbFramebuffer ? "Linear" : "NonLinear", LocalConfig::MenuAtlas);
    }

    //! Break a text into lines at spaces (a label shows one line)
    std::vector<std::string> WrapText(const std::string& text, const std::size_t maxLineLength)
    {
      std::vector<std::string> lines;
      std::string line;
      std::size_t start = 0;
      while (start < text.size())
      {
        std::size_t end = text.find(' ', start);
        if (end == std::string::npos)
        {
          end = text.size();
        }
        std::string word = text.substr(start, end - start);
        while (word.size() > maxLineLength)
        {
          if (!line.empty())
          {
            lines.push_back(std::move(line));
            line.clear();
          }
          lines.push_back(word.substr(0, maxLineLength));
          word = word.substr(maxLineLength);
        }
        if (!line.empty() && (line.size() + 1 + word.size()) > maxLineLength)
        {
          lines.push_back(std::move(line));
          line.clear();
        }
        if (!line.empty())
        {
          line += ' ';
        }
        line += word;
        start = end + 1;
      }
      if (!line.empty())
      {
        lines.push_back(std::move(line));
      }
      return lines;
    }

    //! iDate: the date and time in UTC
    std::array<float, 4> GetDate()
    {
      const auto now = std::chrono::system_clock::now();
      const auto today = std::chrono::floor<std::chrono::days>(now);
      const std::chrono::year_month_day date(today);
      const std::chrono::duration<double> secondsToday = now - today;
      return {static_cast<float>(static_cast<int>(date.year())), static_cast<float>(static_cast<unsigned>(date.month()) - 1u),
              static_cast<float>(static_cast<unsigned>(date.day())), static_cast<float>(secondsToday.count())};
    }

    //! The scene the command line asks for: the name of its folder or its number (1 is the first)
    uint32_t FindStartScene(const std::vector<std::string>& sceneIds, const std::string& scene)
    {
      if (scene.empty())
      {
        return 0;
      }
      for (uint32_t i = 0; i < sceneIds.size(); ++i)
      {
        if (sceneIds[i] == scene)
        {
          return i;
        }
      }
      if (std::all_of(scene.begin(), scene.end(), [](const char ch) { return ch >= '0' && ch <= '9'; }))
      {
        const auto number = std::stoul(scene);
        if (number >= 1 && number <= sceneIds.size())
        {
          return static_cast<uint32_t>(number - 1);
        }
      }
      FSLLOG3_WARNING("There is no scene '{}', starting with the first scene", scene);
      return 0;
    }
  }


  PixelArtShared::PixelArtShared(const DemoAppConfig& config, IPixelArtSceneRenderer& rRenderer, const bool srgbFramebuffer)
    : m_uiEventListener(this)
    , m_uiExtension(std::make_shared<UIDemoAppExtension>(
        config, m_uiEventListener.GetListener(), GetAtlasName(srgbFramebuffer),
        UIDemoAppExtension::CreateConfig(srgbFramebuffer ? UI::UIColorSpace::SRGBLinear : UI::UIColorSpace::SRGBNonLinear)))
    , m_contentManager(config.DemoServiceProvider.Get<IContentManager>())
    , m_renderer(rRenderer)
    , m_windowSizePx(config.WindowMetrics.GetSizePx())
    , m_gpuChartData(std::make_shared<UI::ChartData>(m_uiExtension->GetDataBinding(), config.WindowMetrics.ExtentPx.Width.Value, 1,
                                                     UI::ChartData::Constraints(0, {})))
  {
    const auto options = config.GetOptions<PixelArtOptionParser>();
    const auto uiFactory = UI::Theme::ThemeSelector::CreateControlFactory(*m_uiExtension);
    auto& rFactory = *uiFactory;

    // The scene switcher: it stays at the top of the panel, the params of the scene scroll below it
    m_ui.ButtonPrevScene = rFactory.CreateTextButton(UI::Theme::ButtonType::Text, "<");
    m_ui.ButtonNextScene = rFactory.CreateTextButton(UI::Theme::ButtonType::Text, ">");
    m_ui.LabelSceneName = rFactory.CreateLabel("", UI::Theme::FontType::Header);
    m_ui.LabelSceneName->SetAlignmentX(UI::ItemAlignment::Center);
    m_ui.LabelSceneName->SetAlignmentY(UI::ItemAlignment::Center);
    const auto switcherLayout = std::make_shared<UI::GridLayout>(rFactory.GetContext());
    switcherLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    switcherLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    switcherLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    switcherLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    switcherLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Auto));
    switcherLayout->AddChild(m_ui.ButtonPrevScene, 0, 0);
    switcherLayout->AddChild(m_ui.LabelSceneName, 1, 0);
    switcherLayout->AddChild(m_ui.ButtonNextScene, 2, 0);
    m_ui.LabelSceneNumber = rFactory.CreateLabel("");
    m_ui.LabelSceneNumber->SetAlignmentX(UI::ItemAlignment::Center);
    m_ui.ErrorLayout = std::make_shared<UI::StackLayout>(rFactory.GetContext());
    m_ui.ErrorLayout->SetOrientation(UI::LayoutOrientation::Vertical);
    m_ui.ErrorLayout->SetVisibility(UI::ItemVisibility::Collapsed);

    const auto headerLayout = std::make_shared<UI::StackLayout>(rFactory.GetContext());
    headerLayout->SetOrientation(UI::LayoutOrientation::Vertical);
    headerLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    headerLayout->AddChild(switcherLayout);
    headerLayout->AddChild(m_ui.LabelSceneNumber);
    headerLayout->AddChild(m_ui.ErrorLayout);
    headerLayout->AddChild(rFactory.CreateDivider(UI::LayoutOrientation::Horizontal));

    // The params of the scene (filled by RebuildParamUI)
    m_ui.ParamLayout = std::make_shared<UI::StackLayout>(rFactory.GetContext());
    m_ui.ParamLayout->SetOrientation(UI::LayoutOrientation::Vertical);
    m_ui.ParamLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.ButtonReset = rFactory.CreateTextButton(UI::Theme::ButtonType::Contained, "Reset to defaults");
    m_ui.ButtonReset->SetAlignmentX(UI::ItemAlignment::Stretch);

    // The player: the same for every scene
    const auto lblRenderScale = rFactory.CreateLabel("Render scale");
    m_ui.SliderRenderScale = rFactory.CreateSliderFmtValue(
      UI::LayoutOrientation::Horizontal,
      ConstrainedValue<float>(options->GetRenderScale(), LocalConfig::RenderScale.Min(), LocalConfig::RenderScale.Max()), "{:.2f}");
    m_ui.SliderRenderScale->SetTickFrequency(LocalConfig::RenderScaleStep);
    m_ui.SliderRenderScale->SetAlignmentX(UI::ItemAlignment::Stretch);
    const auto lblTimeSpeed = rFactory.CreateLabel("Time speed");
    m_ui.SliderTimeSpeed = rFactory.CreateSliderFmtValue(
      UI::LayoutOrientation::Horizontal, ConstrainedValue<float>(options->GetTimeSpeed(), LocalConfig::TimeSpeed.Min(), LocalConfig::TimeSpeed.Max()),
      "{:.2f}");
    m_ui.SliderTimeSpeed->SetTickFrequency(LocalConfig::TimeSpeedStep);
    m_ui.SliderTimeSpeed->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.SwitchPause = rFactory.CreateSwitch("Pause (Space)", options->IsPaused());
    m_ui.SwitchGpuChart = rFactory.CreateSwitch("GPU time chart (G)", options->IsGpuChartEnabled());
    m_ui.ButtonRestart = rFactory.CreateTextButton(UI::Theme::ButtonType::Contained, "Restart (Backspace)");
    m_ui.ButtonRestart->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.ButtonReload = rFactory.CreateTextButton(UI::Theme::ButtonType::Contained, "Reload the scene (F5)");
    m_ui.ButtonReload->SetAlignmentX(UI::ItemAlignment::Stretch);
    m_ui.ButtonHideUI = rFactory.CreateTextButton(UI::Theme::ButtonType::Outlined, "Hide the UI (U)");
    m_ui.ButtonHideUI->SetAlignmentX(UI::ItemAlignment::Stretch);
    const auto lblHintScene = rFactory.CreateLabel("Left/Right: switch scene");
    const auto lblHintMouse = rFactory.CreateLabel("Drag: iMouse (orbit)");
    const auto lblHintReset = rFactory.CreateLabel("R: reset the params");

    const auto scrollLayout = std::make_shared<UI::StackLayout>(rFactory.GetContext());
    scrollLayout->SetOrientation(UI::LayoutOrientation::Vertical);
    scrollLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    scrollLayout->SetMargin(DpThicknessF::Create(0, 0, 8, 0));
    scrollLayout->AddChild(m_ui.ParamLayout);
    scrollLayout->AddChild(m_ui.ButtonReset);
    scrollLayout->AddChild(rFactory.CreateDivider(UI::LayoutOrientation::Horizontal));
    scrollLayout->AddChild(rFactory.CreateLabel("Player", UI::Theme::FontType::Header));
    scrollLayout->AddChild(lblRenderScale);
    scrollLayout->AddChild(m_ui.SliderRenderScale);
    scrollLayout->AddChild(lblTimeSpeed);
    scrollLayout->AddChild(m_ui.SliderTimeSpeed);
    scrollLayout->AddChild(m_ui.SwitchPause);
    scrollLayout->AddChild(m_ui.SwitchGpuChart);
    scrollLayout->AddChild(m_ui.ButtonRestart);
    scrollLayout->AddChild(m_ui.ButtonReload);
    scrollLayout->AddChild(m_ui.ButtonHideUI);
    scrollLayout->AddChild(rFactory.CreateDivider(UI::LayoutOrientation::Horizontal));
    scrollLayout->AddChild(lblHintScene);
    scrollLayout->AddChild(lblHintMouse);
    scrollLayout->AddChild(lblHintReset);
    const auto scrollViewer = rFactory.CreateScrollViewer(scrollLayout, UI::ScrollModeFlags::TranslateY, false);
    scrollViewer->SetAlignmentX(UI::ItemAlignment::Stretch);
    scrollViewer->SetAlignmentY(UI::ItemAlignment::Stretch);

    const auto panelLayout = std::make_shared<UI::GridLayout>(rFactory.GetContext());
    panelLayout->SetWidth(UI::DpLayoutSize1D::Create(LocalConfig::PanelWidthDp));
    panelLayout->SetAlignmentY(UI::ItemAlignment::Stretch);
    panelLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    panelLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Auto));
    panelLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    panelLayout->AddChild(headerLayout, 0, 0);
    panelLayout->AddChild(scrollViewer, 0, 1);
    // The InputCatcher takes every click on the panel (labels, the background), so a click on it never reaches the scene (iMouse)
    const auto rightBarInputCatcher = std::make_shared<UI::InputCatcher>(rFactory.GetContext());
    rightBarInputCatcher->SetAlignmentX(UI::ItemAlignment::Far);
    rightBarInputCatcher->SetAlignmentY(UI::ItemAlignment::Stretch);
    rightBarInputCatcher->SetContent(rFactory.CreateRightBar(panelLayout));
    m_ui.RightBar = rightBarInputCatcher;

    // Shown while the panel is hidden
    m_ui.ButtonShowUI = rFactory.CreateTextButton(UI::Theme::ButtonType::Contained, "UI");
    m_ui.ShowUIBar = m_ui.ButtonShowUI;
    m_ui.ShowUIBar->SetAlignmentX(UI::ItemAlignment::Far);
    m_ui.ShowUIBar->SetAlignmentY(UI::ItemAlignment::Near);
    m_ui.ShowUIBar->SetMargin(DpThicknessF::Create(8, 8, 8, 8));

    const auto mainLayout = std::make_shared<UI::GridLayout>(rFactory.GetContext());
    mainLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    mainLayout->SetAlignmentY(UI::ItemAlignment::Stretch);
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    mainLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    // The scene area: the button that shows the panel at the top, the GPU chart at the bottom
    const auto sceneLayout = std::make_shared<UI::GridLayout>(rFactory.GetContext());
    sceneLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    sceneLayout->SetAlignmentY(UI::ItemAlignment::Stretch);
    sceneLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    sceneLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    sceneLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Auto));
    sceneLayout->AddChild(m_ui.ShowUIBar, 0, 0);
    m_ui.GpuChartBar = CreateGpuChartBar(rFactory);
    sceneLayout->AddChild(m_ui.GpuChartBar, 0, 1);
    mainLayout->AddChild(sceneLayout, 0, 0);
    mainLayout->AddChild(m_ui.RightBar, 1, 0);
    mainLayout->SetLimitToAvailableSpace(true);
    m_uiExtension->SetMainWindow(mainLayout);
    m_uiFactory = uiFactory;

    SetUIVisible(!options->IsUIHidden());

    // The scenes
    const std::vector<std::string> sceneIds = PixelArtSceneLoader::LoadSceneList(*m_contentManager);
    m_scenes.reserve(sceneIds.size());
    for (const std::string& sceneId : sceneIds)
    {
      SceneRecord record;
      record.Id = sceneId;
      record.Name = sceneId;
      m_scenes.push_back(std::move(record));
    }

    const uint32_t startScene = FindStartScene(sceneIds, options->GetScene());
    SelectScene(startScene);
    // If the scene could not be loaded the first one that can be is shown, so there is something on the screen
    for (uint32_t i = 0; i < m_scenes.size() && !m_activeScene.has_value(); ++i)
    {
      if (i != startScene)
      {
        SelectScene(i);
      }
    }
    if (m_activeScene.has_value() && m_activeScene.value() == startScene)
    {
      SceneRecord& rScene = m_scenes[startScene];
      for (const auto& param : options->GetParams())
      {
        if (!rScene.Params.TrySetValue(param.first, param.second))
        {
          FSLLOG3_WARNING("The scene '{}' has no param named '{}'", rScene.Id, param.first);
        }
      }
      RebuildParamUI();
    }
    m_time = options->GetStartTime();
  }


  PixelArtShared::~PixelArtShared() = default;


  void PixelArtShared::OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent)
  {
    if (theEvent->IsHandled())
    {
      return;
    }
    const auto& source = theEvent->GetSource();
    if (source == m_ui.ButtonPrevScene)
    {
      theEvent->Handled();
      SelectNextScene(-1);
    }
    else if (source == m_ui.ButtonNextScene)
    {
      theEvent->Handled();
      SelectNextScene(1);
    }
    else if (source == m_ui.ButtonReset)
    {
      theEvent->Handled();
      ResetParams();
    }
    else if (source == m_ui.ButtonRestart)
    {
      theEvent->Handled();
      Restart();
    }
    else if (source == m_ui.ButtonReload)
    {
      theEvent->Handled();
      SelectScene(m_selectedScene);
    }
    else if (source == m_ui.ButtonHideUI)
    {
      theEvent->Handled();
      SetUIVisible(false);
    }
    else if (source == m_ui.ButtonShowUI)
    {
      theEvent->Handled();
      SetUIVisible(true);
    }
  }


  void PixelArtShared::OnKeyEvent(const KeyEvent& event)
  {
    if (event.IsHandled() || !event.IsPressed())
    {
      return;
    }
    switch (event.GetKey())
    {
    case VirtualKey::LeftArrow:
      event.Handled();
      SelectNextScene(-1);
      break;
    case VirtualKey::RightArrow:
      event.Handled();
      SelectNextScene(1);
      break;
    case VirtualKey::Space:
      event.Handled();
      m_ui.SwitchPause->Toggle();
      break;
    case VirtualKey::Backspace:
      event.Handled();
      Restart();
      break;
    case VirtualKey::R:
      event.Handled();
      ResetParams();
      break;
    case VirtualKey::G:
      event.Handled();
      m_ui.SwitchGpuChart->Toggle();
      break;
    case VirtualKey::U:
      event.Handled();
      SetUIVisible(m_ui.RightBar->GetVisibility() != UI::ItemVisibility::Visible);
      break;
    case VirtualKey::F5:
      event.Handled();
      SelectScene(m_selectedScene);
      break;
    default:
      break;
    }
  }


  void PixelArtShared::OnMouseButtonEvent(const MouseButtonEvent& event)
  {
    if (event.GetButton() != VirtualMouseButton::Left)
    {
      return;
    }
    const auto position = event.GetPosition();
    const auto x = static_cast<float>(position.X.Value);
    // shadertoy has its origin in the bottom left corner
    const auto y = static_cast<float>(m_windowSizePx.RawHeight() - position.Y.Value);
    if (event.IsPressed())
    {
      // A press the UI used is not a drag of the scene
      if (event.IsHandled())
      {
        return;
      }
      event.Handled();
      m_mouse.IsDragging = true;
      // z and w are positive: the button is down and it went down this frame
      m_mouse.Value = {x, y, x, y};
    }
    else if (m_mouse.IsDragging)
    {
      // A release always ends the drag, even if the UI used it
      m_mouse.IsDragging = false;
      m_mouse.Value[2] = -std::abs(m_mouse.Value[2]);
      m_mouse.Value[3] = -std::abs(m_mouse.Value[3]);
    }
  }


  void PixelArtShared::OnMouseMoveEvent(const MouseMoveEvent& event)
  {
    if (!m_mouse.IsDragging)
    {
      return;
    }
    event.Handled();
    const auto position = event.GetPosition();
    m_mouse.Value[0] = static_cast<float>(position.X.Value);
    m_mouse.Value[1] = static_cast<float>(m_windowSizePx.RawHeight() - position.Y.Value);
  }


  void PixelArtShared::ConfigurationChanged(const DemoWindowMetrics& windowMetrics)
  {
    m_windowSizePx = windowMetrics.GetSizePx();
  }


  void PixelArtShared::Update(const DemoTime& demoTime)
  {
    ApplyParamUI();

    UpdateGpuChartVisibility();

    const bool paused = m_ui.SwitchPause->IsChecked();
    const float timeDelta = paused ? 0.0f : demoTime.DeltaTime * m_ui.SliderTimeSpeed->GetValue();
    m_time += timeDelta;

    m_frameState.Time = static_cast<float>(m_time);
    m_frameState.TimeDelta = timeDelta;
    m_frameState.FrameRate = demoTime.DeltaTime > 0.0f ? (1.0f / demoTime.DeltaTime) : 0.0f;
    m_frameState.Frame = m_frame;
    m_frameState.MouseWindowPx = m_mouse.Value;
    m_frameState.Date = GetDate();
    m_frameState.SceneSizePx = GetSceneSizePx();
    m_frameState.RenderSizePx = GetRenderSizePx(m_frameState.SceneSizePx);
    m_frameState.Params = m_activeScene.has_value() ? m_scenes[m_activeScene.value()].Params.GetPackedValues() : ReadOnlySpan<float>();

    ++m_frame;
    // iMouse.w is only positive on the frame the button went down
    m_mouse.Value[3] = -std::abs(m_mouse.Value[3]);
  }


  void PixelArtShared::Draw()
  {
    m_uiExtension->Draw();
  }


  void PixelArtShared::SelectScene(const uint32_t sceneIndex)
  {
    SceneRecord& rScene = m_scenes.at(sceneIndex);
    try
    {
      const PixelArtSceneDesc scene = PixelArtSceneLoader::LoadScene(*m_contentManager, rScene.Id);
      m_renderer.LoadScene(scene);

      // A scene that was shown before keeps its values (a reload keeps the values of the params that still exist)
      PixelArtParamSet params(scene.Params);
      if (rScene.Loaded)
      {
        params.CopyValuesByName(rScene.Params);
      }
      rScene.Params = std::move(params);
      rScene.Name = scene.Name;
      rScene.Error.clear();
      rScene.Loaded = true;
      m_activeScene = sceneIndex;
      FSLLOG3_INFO("Scene '{}' loaded", rScene.Id);
      Restart();
    }
    catch (const std::exception& ex)
    {
      rScene.Error = ex.what();
      FSLLOG3_ERROR("Scene '{}' could not be loaded: {}", rScene.Id, rScene.Error);
    }
    m_selectedScene = sceneIndex;
    RebuildParamUI();
    UpdateSceneLabels();
  }


  void PixelArtShared::SelectNextScene(const int32_t direction)
  {
    const auto count = static_cast<int32_t>(m_scenes.size());
    const int32_t index = ((static_cast<int32_t>(m_selectedScene) + direction) % count + count) % count;
    SelectScene(static_cast<uint32_t>(index));
  }


  void PixelArtShared::Restart()
  {
    m_time = 0.0;
    m_frame = 0;
  }


  void PixelArtShared::ResetParams()
  {
    if (!m_activeScene.has_value() || m_activeScene.value() != m_selectedScene)
    {
      return;
    }
    PixelArtParamSet& rParams = m_scenes[m_selectedScene].Params;
    rParams.ResetToDefaults();
    for (const ParamUIRecord& record : m_ui.Params)
    {
      record.Slider->SetValue(rParams.GetValue(record.Index));
    }
  }


  void PixelArtShared::SetUIVisible(const bool visible)
  {
    m_ui.RightBar->SetVisibility(visible ? UI::ItemVisibility::Visible : UI::ItemVisibility::Collapsed);
    m_ui.ShowUIBar->SetVisibility(visible ? UI::ItemVisibility::Collapsed : UI::ItemVisibility::Visible);
    UpdateGpuChartVisibility();
  }


  void PixelArtShared::SetGpuTimerSupported(const bool supported)
  {
    m_isGpuTimerSupported = supported;
    m_ui.LabelGpuChartStatus->SetContent(supported ? "" : "not available");
    m_ui.LabelGpuChartStatus->SetVisibility(supported ? UI::ItemVisibility::Collapsed : UI::ItemVisibility::Visible);
  }


  void PixelArtShared::AddGpuTime(const TimeSpan gpuTime)
  {
    if (!m_ui.SwitchGpuChart->IsChecked())
    {
      return;
    }
    const int64_t microseconds = gpuTime.Ticks() / TimeSpan::TicksPerMicrosecond;
    UI::ChartDataEntry entry;
    entry.Values[0] = static_cast<uint32_t>(std::clamp(microseconds, int64_t{0}, int64_t{std::numeric_limits<uint32_t>::max()}));
    m_gpuChartData->Append(entry);
  }


  void PixelArtShared::UpdateGpuChartVisibility()
  {
    const bool visible = m_ui.SwitchGpuChart->IsChecked() && m_ui.RightBar->GetVisibility() == UI::ItemVisibility::Visible;
    const UI::ItemVisibility visibility = visible ? UI::ItemVisibility::Visible : UI::ItemVisibility::Collapsed;
    if (m_ui.GpuChartBar->GetVisibility() != visibility)
    {
      m_ui.GpuChartBar->SetVisibility(visibility);
    }
  }


  std::shared_ptr<UI::BaseWindow> PixelArtShared::CreateGpuChartBar(UI::Theme::IThemeControlFactory& rFactory)
  {
    const auto context = rFactory.GetContext();
    m_gpuChartData->SetChannelMetaData(0, LocalConfig::GpuChartColor);

    const auto chart = std::make_shared<UI::AreaChart>(context);
    chart->SetAlignmentX(UI::ItemAlignment::Stretch);
    chart->SetAlignmentY(UI::ItemAlignment::Stretch);
    chart->SetOpaqueFillSprite(rFactory.GetResources().GetBasicFillSprite(true));
    chart->SetTransparentFillSprite(rFactory.GetResources().GetBasicFillSprite(false));
    chart->SetGridLines(std::make_unique<UI::ChartGridLinesFps>());
    chart->SetDataView(m_gpuChartData);
    chart->SetFont(rFactory.GetResources().GetDefaultSpriteFont());
    chart->SetLabelBackground(rFactory.GetResources().GetToolTipNineSliceSprite());
    chart->SetRenderPolicy(UI::ChartRenderPolicy::FillAvailable);

    const auto labelGpu = rFactory.CreateLabel("GPU");
    labelGpu->SetFontColor(LocalConfig::GpuChartColor);
    m_ui.LabelGpuChartStatus = rFactory.CreateLabel("");
    m_ui.LabelGpuChartStatus->SetVisibility(UI::ItemVisibility::Collapsed);
    const auto legend = std::make_shared<UI::StackLayout>(context);
    legend->SetOrientation(UI::LayoutOrientation::Vertical);
    legend->SetAlignmentY(UI::ItemAlignment::Center);
    legend->AddChild(rFactory.CreateLabel("Time per frame"));
    legend->AddChild(labelGpu);
    legend->AddChild(m_ui.LabelGpuChartStatus);

    const auto grid = std::make_shared<UI::GridLayout>(context);
    grid->SetAlignmentX(UI::ItemAlignment::Stretch);
    grid->SetMargin(DpThicknessF::Create(8, 0, 8, 0));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Fixed, 8));
    grid->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    grid->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Fixed, LocalConfig::GpuChartHeightDp));
    grid->AddChild(legend, 0, 0);
    grid->AddChild(chart, 2, 0);

    // A click on the chart is not a drag of the scene
    const auto inputCatcher = std::make_shared<UI::InputCatcher>(context);
    inputCatcher->SetAlignmentX(UI::ItemAlignment::Stretch);
    inputCatcher->SetAlignmentY(UI::ItemAlignment::Far);
    inputCatcher->SetContent(rFactory.CreateBottomBar(grid, UI::Theme::BarType::Transparent));
    inputCatcher->SetVisibility(UI::ItemVisibility::Collapsed);
    return inputCatcher;
  }


  void PixelArtShared::RebuildParamUI()
  {
    m_ui.ParamLayout->ClearChildren();
    m_ui.Params.clear();
    m_ui.Groups.clear();
    m_ui.ButtonReset->SetEnabled(false);
    // Only the scene the renderer draws has params to adjust
    if (!m_activeScene.has_value() || m_activeScene.value() != m_selectedScene || !m_uiFactory)
    {
      return;
    }
    auto& rFactory = *m_uiFactory;
    const PixelArtParamSet& params = m_scenes[m_selectedScene].Params;

    // The groups in the order they are first used, the params of a group in the order they are listed
    std::map<std::string, std::size_t> groupIndices;
    for (uint32_t i = 0; i < params.GetCount(); ++i)
    {
      const PixelArtParamDesc& desc = params.GetDesc(i);
      auto itr = groupIndices.find(desc.Group);
      if (itr == groupIndices.end())
      {
        GroupUIRecord group;
        group.Header = rFactory.CreateSwitch(desc.Group, true);
        group.Header->SetAlignmentX(UI::ItemAlignment::Stretch);
        group.Content = std::make_shared<UI::StackLayout>(rFactory.GetContext());
        group.Content->SetOrientation(UI::LayoutOrientation::Vertical);
        group.Content->SetAlignmentX(UI::ItemAlignment::Stretch);
        group.Content->SetMargin(DpThicknessF::Create(8, 0, 0, 4));
        m_ui.ParamLayout->AddChild(group.Header);
        m_ui.ParamLayout->AddChild(group.Content);
        itr = groupIndices.emplace(desc.Group, m_ui.Groups.size()).first;
        m_ui.Groups.push_back(std::move(group));
      }
      const GroupUIRecord& group = m_ui.Groups[itr->second];

      auto slider = rFactory.CreateSliderFmtValue(UI::LayoutOrientation::Horizontal, ConstrainedValue<float>(params.GetValue(i), desc.Min, desc.Max),
                                                  desc.Format);
      if (desc.Step > 0.0f)
      {
        slider->SetTickFrequency(desc.Step);
      }
      slider->SetAlignmentX(UI::ItemAlignment::Stretch);
      group.Content->AddChild(rFactory.CreateLabel(desc.Label));
      group.Content->AddChild(slider);
      m_ui.Params.push_back(ParamUIRecord{i, std::move(slider)});
    }
    if (params.GetCount() == 0)
    {
      m_ui.ParamLayout->AddChild(rFactory.CreateLabel("The scene has no params"));
    }
    m_ui.ButtonReset->SetEnabled(params.GetCount() > 0);
  }


  void PixelArtShared::UpdateSceneLabels()
  {
    const SceneRecord& scene = m_scenes[m_selectedScene];
    m_ui.LabelSceneName->SetContent(scene.Name);
    m_ui.LabelSceneNumber->SetContent(fmt::format("Scene {} of {}", m_selectedScene + 1, m_scenes.size()));
    m_ui.ErrorLayout->ClearChildren();
    if (scene.Error.empty() || !m_uiFactory)
    {
      m_ui.ErrorLayout->SetVisibility(UI::ItemVisibility::Collapsed);
      return;
    }
    auto& rFactory = *m_uiFactory;
    m_ui.ErrorLayout->AddChild(rFactory.CreateLabel("Could not be loaded:"));
    for (const std::string& line : WrapText(scene.Error, LocalConfig::ErrorLineLength))
    {
      m_ui.ErrorLayout->AddChild(rFactory.CreateLabel(line));
    }
    if (m_activeScene.has_value())
    {
      m_ui.ErrorLayout->AddChild(rFactory.CreateLabel(fmt::format("Still showing '{}'", m_scenes[m_activeScene.value()].Name)));
    }
    m_ui.ErrorLayout->SetVisibility(UI::ItemVisibility::Visible);
  }


  void PixelArtShared::ApplyParamUI()
  {
    for (const GroupUIRecord& group : m_ui.Groups)
    {
      group.Content->SetVisibility(group.Header->IsChecked() ? UI::ItemVisibility::Visible : UI::ItemVisibility::Collapsed);
    }
    if (!m_activeScene.has_value() || m_activeScene.value() != m_selectedScene)
    {
      return;
    }
    PixelArtParamSet& rParams = m_scenes[m_selectedScene].Params;
    for (const ParamUIRecord& record : m_ui.Params)
    {
      rParams.SetValue(record.Index, record.Slider->GetValue());
    }
  }


  PxSize2D PixelArtShared::GetSceneSizePx() const
  {
    // The scene is shown left of the UI panel (it uses the width the panel got in the last layout), so no part of it is under the panel
    int32_t panelWidthPx = 0;
    if (m_ui.RightBar->GetVisibility() == UI::ItemVisibility::Visible)
    {
      panelWidthPx = m_ui.RightBar->RenderSizePx().RawWidth();
    }
    return PxSize2D::Create(std::max(m_windowSizePx.RawWidth() - panelWidthPx, int32_t{1}), std::max(m_windowSizePx.RawHeight(), int32_t{1}));
  }


  PxSize2D PixelArtShared::GetRenderSizePx(const PxSize2D sceneSizePx) const
  {
    const float scale = m_ui.SliderRenderScale->GetValue();
    const auto width = std::max(static_cast<int32_t>(std::lround(static_cast<float>(sceneSizePx.RawWidth()) * scale)), int32_t{1});
    const auto height = std::max(static_cast<int32_t>(std::lround(static_cast<float>(sceneSizePx.RawHeight()) * scale)), int32_t{1});
    return PxSize2D::Create(width, height);
  }
}
