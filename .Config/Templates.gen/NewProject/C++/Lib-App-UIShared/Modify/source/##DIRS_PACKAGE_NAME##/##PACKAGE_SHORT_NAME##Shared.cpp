/****************************************************************************************************************************************************
* Copyright ##PACKAGE_CREATION_YEAR## ##PACKAGE_COMPANY_NAME##
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
*    * Neither the name of the ##PACKAGE_COMPANY_NAME##. nor the names of
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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoApp/Base/Service/Content/IContentManager.hpp>
#include <FslGraphics/Colors.hpp>
#include <FslGraphics/Sprite/Font/SpriteFont.hpp>
#include <FslSimpleUI/App/Theme/ThemeSelector.hpp>
#include <FslSimpleUI/Base/Control/Background.hpp>
#include <FslSimpleUI/Base/Control/Image.hpp>
#include <FslSimpleUI/Base/Control/Label.hpp>
#include <FslSimpleUI/Base/Control/ScrollViewer.hpp>
#include <FslSimpleUI/Base/Event/WindowSelectEvent.hpp>
#include <FslSimpleUI/Base/Layout/FillLayout.hpp>
#include <FslSimpleUI/Base/Layout/GridLayout.hpp>
#include <FslSimpleUI/Base/Layout/StackLayout.hpp>
#include <FslSimpleUI/Theme/Base/IThemeControlFactory.hpp>
#include <##DIRS_PACKAGE_NAME##/##PACKAGE_SHORT_NAME##Shared.hpp>
#include <cassert>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr IO::PathView MenuAtlas("UIAtlas/UIAtlas_160dpi");
    }
  }

  ##PACKAGE_SHORT_NAME##Shared::##PACKAGE_SHORT_NAME##Shared(const DemoAppConfig& config)
    : m_uiEventListener(this)
    , m_uiExtension(std::make_shared<UIDemoAppExtension>(config, m_uiEventListener.GetListener(), LocalConfig::MenuAtlas))
  {
    auto uiControlFactory = UI::Theme::ThemeSelector::CreateControlFactory(*m_uiExtension);

    m_ui = CreateUI(*uiControlFactory);
    m_uiExtension->SetMainWindow(m_ui.MainLayout);


    SetDefaults();
    ConfigurationChanged(config.WindowMetrics);
  }


  ##PACKAGE_SHORT_NAME##Shared::~##PACKAGE_SHORT_NAME##Shared() = default;


  void ##PACKAGE_SHORT_NAME##Shared::OnKeyEvent(const KeyEvent& event)
  {
    if (event.IsHandled() || !event.IsPressed())
    {
      return;
    }

    switch (event.GetKey())
    {
    case VirtualKey::Space:
      SetDefaults();
      break;
    default:
      break;
    }
  }

  void ##PACKAGE_SHORT_NAME##Shared::OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent)
  {
    if (theEvent->IsHandled())
    {
      return;
    }

    if (theEvent->GetSource() == m_ui.MenuContent.BtnSetDefaults)
    {
      theEvent->Handled();
      SetDefaults();
    }
    else if (theEvent->GetSource() == m_ui.MenuContent.Btn0)
    {
      theEvent->Handled();
      FSLLOG3_INFO("Button 0 pressed");
    }
    else if (theEvent->GetSource() == m_ui.MenuContent.Btn1)
    {
      theEvent->Handled();
      FSLLOG3_INFO("Button 1 pressed");
    }
    else if (theEvent->GetSource() == m_ui.MenuContent.Btn2)
    {
      theEvent->Handled();
      FSLLOG3_INFO("Button 2 pressed");
    }
    else if (theEvent->GetSource() == m_ui.Content.BtnHelloWorld)
    {
      theEvent->Handled();
      FSLLOG3_INFO("Button hello world pressed");
    }
  }

  void ##PACKAGE_SHORT_NAME##Shared::ConfigurationChanged(const DemoWindowMetrics& windowMetrics)
  {
    if (windowMetrics == m_cachedWindowMetrics)
    {
      return;
    }
    m_cachedWindowMetrics = windowMetrics;
  }

  void ##PACKAGE_SHORT_NAME##Shared::Update(const DemoTime& demoTime)
  {
    FSL_PARAM_NOT_USED(demoTime);
  }


  void ##PACKAGE_SHORT_NAME##Shared::Draw()
  {
    m_uiExtension->Draw();
  }


  void ##PACKAGE_SHORT_NAME##Shared::SetDefaults()
  {
    FSLLOG3_INFO("Setting default values");
  }

  ##PACKAGE_SHORT_NAME##Shared::UIRecord ##PACKAGE_SHORT_NAME##Shared::CreateUI(UI::Theme::IThemeControlFactory& uiFactory)
  {
    auto menuContent = CreateMenuContentUI(uiFactory);
    auto content = CreateContentUI(uiFactory);

    auto menuSidebar = uiFactory.CreateLeftBar(menuContent.MainLayout);

    // Then finally use a grid so we can adapt to the window size and still limit it to the available size
    auto mainLayout = std::make_shared<UI::GridLayout>(uiFactory.GetContext());
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    mainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Star, 1.0f));
    mainLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    mainLayout->AddChild(content.MainLayout, 1, 0);
    mainLayout->AddChild(menuSidebar, 0, 0);
    mainLayout->SetLimitToAvailableSpace(true);

    return {std::move(mainLayout), std::move(menuContent), std::move(content)};
  }


  ##PACKAGE_SHORT_NAME##Shared::UIMenuContentRecord ##PACKAGE_SHORT_NAME##Shared::CreateMenuContentUI(UI::Theme::IThemeControlFactory& uiFactory)
  {
    // --- Menu top area

    // Create the top buttons
    auto btnButton0 = uiFactory.CreateTextButton(UI::Theme::ButtonType::Contained, "Button0");
    auto btnButton1 = uiFactory.CreateTextButton(UI::Theme::ButtonType::Contained, "Button1");
    auto btnButton2 = uiFactory.CreateTextButton(UI::Theme::ButtonType::Contained, "Button2");
    btnButton0->SetAlignmentX(UI::ItemAlignment::Stretch);
    btnButton1->SetAlignmentX(UI::ItemAlignment::Stretch);
    btnButton2->SetAlignmentX(UI::ItemAlignment::Stretch);

    // Use a simple stack layout for the buttons
    auto menuTopLayout = std::make_shared<UI::StackLayout>(uiFactory.GetContext());
    menuTopLayout->SetOrientation(UI::LayoutOrientation::Vertical);
    menuTopLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    menuTopLayout->SetAlignmentY(UI::ItemAlignment::Center);
    menuTopLayout->AddChild(btnButton0);
    menuTopLayout->AddChild(btnButton1);
    menuTopLayout->AddChild(btnButton2);

    // Add a scroll viewer for the buttons
    auto menuScrollViewer = uiFactory.CreateScrollViewer(menuTopLayout, UI::ScrollModeFlags::TranslateY, true);

    // --- Menu bottom area

    // Create the set default button
    auto btnSetDefaults = uiFactory.CreateTextButton(UI::Theme::ButtonType::Contained, "Set defaults");
    btnSetDefaults->SetAlignmentX(UI::ItemAlignment::Center);
    btnSetDefaults->SetAlignmentY(UI::ItemAlignment::Center);

    // Use a simple stack layout so we have a divider and the set defaults button
    auto menuBottomLayout = std::make_shared<UI::StackLayout>(uiFactory.GetContext());
    menuBottomLayout->SetOrientation(UI::LayoutOrientation::Vertical);
    menuBottomLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    menuBottomLayout->SetAlignmentY(UI::ItemAlignment::Center);
    menuBottomLayout->AddChild(uiFactory.CreateDivider(UI::LayoutOrientation::Horizontal));
    menuBottomLayout->AddChild(btnSetDefaults);

    // Then finally use a grid so we can adapt to the window size and still limit it to the available size
    auto menuMainLayout = std::make_shared<UI::GridLayout>(uiFactory.GetContext());
    menuMainLayout->AddColumnDefinition(UI::GridColumnDefinition(UI::GridUnitType::Auto));
    menuMainLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Star, 1.0f));
    menuMainLayout->AddRowDefinition(UI::GridRowDefinition(UI::GridUnitType::Auto));
    menuMainLayout->AddChild(menuScrollViewer, 0, 0);
    menuMainLayout->AddChild(menuBottomLayout, 0, 1);
    menuMainLayout->SetLimitToAvailableSpace(true);

    return {std::move(menuMainLayout), std::move(btnSetDefaults), std::move(btnButton0), std::move(btnButton1), std::move(btnButton2)};
  }


  ##PACKAGE_SHORT_NAME##Shared::UIContentRecord ##PACKAGE_SHORT_NAME##Shared::CreateContentUI(UI::Theme::IThemeControlFactory& uiFactory)
  {
    auto lblTopLeft = uiFactory.CreateLabel("Top left");
    auto lblTopRight = uiFactory.CreateLabel("Top right");
    auto lblBottomLeft = uiFactory.CreateLabel("Bottom left");
    auto lblBottomRight = uiFactory.CreateLabel("Bottom right");

    lblTopLeft->SetAlignmentX(UI::ItemAlignment::Near);
    lblTopLeft->SetAlignmentY(UI::ItemAlignment::Near);

    lblTopRight->SetAlignmentX(UI::ItemAlignment::Far);
    lblTopRight->SetAlignmentY(UI::ItemAlignment::Near);

    lblBottomLeft->SetAlignmentX(UI::ItemAlignment::Near);
    lblBottomLeft->SetAlignmentY(UI::ItemAlignment::Far);

    lblBottomRight->SetAlignmentX(UI::ItemAlignment::Far);
    lblBottomRight->SetAlignmentY(UI::ItemAlignment::Far);

    auto btnHelloWorld = uiFactory.CreateTextButton(UI::Theme::ButtonType::Contained, "Hello world");
    btnHelloWorld->SetAlignmentX(UI::ItemAlignment::Center);
    btnHelloWorld->SetAlignmentY(UI::ItemAlignment::Center);

    auto mainLayout = std::make_shared<UI::FillLayout>(uiFactory.GetContext());
    mainLayout->SetAlignmentX(UI::ItemAlignment::Stretch);
    mainLayout->SetAlignmentY(UI::ItemAlignment::Stretch);
    mainLayout->AddChild(lblTopLeft);
    mainLayout->AddChild(lblTopRight);
    mainLayout->AddChild(lblBottomLeft);
    mainLayout->AddChild(lblBottomRight);
    mainLayout->AddChild(btnHelloWorld);

    return {mainLayout, std::move(btnHelloWorld)};
  }
}

