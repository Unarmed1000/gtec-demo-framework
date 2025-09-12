#ifndef ##{$(PACKAGE_NAME).upper().replace('.','_')}##_HPP
#define ##{$(PACKAGE_NAME).upper().replace('.','_')}##_HPP
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

#include <FslDemoApp/Base/DemoAppConfig.hpp>
#include <FslDemoApp/Base/DemoTime.hpp>
#include <FslGraphics/TextureAtlas/TextureAtlasMap.hpp>
#include <FslSimpleUI/App/UIDemoAppExtension.hpp>
#include <FslSimpleUI/Base/Control/BackgroundLabelButton.hpp>
#include <FslSimpleUI/Theme/Base/IThemeControlFactory.hpp>
#include <memory>
#include <utility>

namespace Fsl
{
  class ##PACKAGE_SHORT_NAME##Shared final : public UI::EventListener
  {
    struct UIContentRecord
    {
      std::shared_ptr<UI::BaseWindow> MainLayout;
      std::shared_ptr<UI::BackgroundLabelButton> BtnHelloWorld;

      UIContentRecord() = default;

      UIContentRecord(std::shared_ptr<UI::BaseWindow> mainLayout, std::shared_ptr<UI::BackgroundLabelButton> btnHelloWorld)
        : MainLayout(std::move(mainLayout))
        , BtnHelloWorld(std::move(btnHelloWorld))
      {
      }
    };

    struct UIMenuContentRecord
    {
      std::shared_ptr<UI::BaseWindow> MainLayout;

      std::shared_ptr<UI::BackgroundLabelButton> BtnSetDefaults;
      std::shared_ptr<UI::BackgroundLabelButton> Btn0;
      std::shared_ptr<UI::BackgroundLabelButton> Btn1;
      std::shared_ptr<UI::BackgroundLabelButton> Btn2;

      UIMenuContentRecord() = default;

      UIMenuContentRecord(std::shared_ptr<UI::BaseWindow> mainLayout, std::shared_ptr<UI::BackgroundLabelButton> btnSetDefaults,
                          std::shared_ptr<UI::BackgroundLabelButton> btn0, std::shared_ptr<UI::BackgroundLabelButton> btn1,
                          std::shared_ptr<UI::BackgroundLabelButton> btn2)
        : MainLayout(std::move(mainLayout))
        , BtnSetDefaults(std::move(btnSetDefaults))
        , Btn0(std::move(btn0))
        , Btn1(std::move(btn1))
        , Btn2(std::move(btn2))
      {
      }
    };

    struct UIRecord
    {
      std::shared_ptr<UI::BaseWindow> MainLayout;
      UIMenuContentRecord MenuContent;
      UIContentRecord Content;

      UIRecord() = default;

      UIRecord(std::shared_ptr<UI::BaseWindow> mainLayout, UIMenuContentRecord menuContent, UIContentRecord content)
        : MainLayout(std::move(mainLayout))
        , MenuContent(std::move(menuContent))
        , Content(std::move(content))
      {
      }
    };

    // The UI event listener is responsible for forwarding events to this classes implementation of the UI::EventListener (while its still alive).
    UI::CallbackEventListenerScope m_uiEventListener;
    // The UIDemoAppExtension is a simple extension that sets up the basic UI framework and listens for the events it needs.
    std::shared_ptr<UIDemoAppExtension> m_uiExtension;

    DemoWindowMetrics m_cachedWindowMetrics;

    UIRecord m_ui;

  public:
    explicit ##PACKAGE_SHORT_NAME##Shared(const DemoAppConfig& config);
    ~##PACKAGE_SHORT_NAME##Shared() override;

    std::shared_ptr<UIDemoAppExtension> GetUIDemoAppExtension() const
    {
      return m_uiExtension;
    }

    // From EventListener
    void OnSelect(const std::shared_ptr<UI::WindowSelectEvent>& theEvent) final;

    // Called from the parent app
    void OnKeyEvent(const KeyEvent& event);
    void ConfigurationChanged(const DemoWindowMetrics& windowMetrics);
    void Update(const DemoTime& demoTime);
    void Draw();

  private:
    void SetDefaults();

    UIRecord CreateUI(UI::Theme::IThemeControlFactory& uiFactory);
    UIMenuContentRecord CreateMenuContentUI(UI::Theme::IThemeControlFactory& uiFactory);
    UIContentRecord CreateContentUI(UI::Theme::IThemeControlFactory& uiFactory);
  };
}
#endif

