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

#include <FslBase/Math/BasicWindowMetrics.hpp>
#include <FslBase/Math/Pixel/PxPoint2.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDataBinding/Base/DataBindingService.hpp>
#include <FslSimpleUI/Base/BaseWindow.hpp>
#include <FslSimpleUI/Base/BaseWindowContext.hpp>
#include <FslSimpleUI/Base/Control/InputCatcher.hpp>
#include <FslSimpleUI/Base/IWindowManager.hpp>
#include <FslSimpleUI/Base/ItemAlignment.hpp>
#include <FslSimpleUI/Base/PxAvailableSize.hpp>
#include <FslSimpleUI/Base/System/UIManager.hpp>
#include <FslSimpleUI/Render/Stub/RenderSystem.hpp>
#include <memory>

using namespace Fsl;

namespace
{
  constexpr PxSize2D ContentSizePx = PxSize2D::Create(200, 100);
  //! A point on the content, which is laid out in the top left corner
  constexpr PxPoint2 OnContentPx = PxPoint2::Create(50, 50);
  //! A point outside the content
  constexpr PxPoint2 OutsidePx = PxPoint2::Create(500, 400);

  //! A window of 200x100 that takes no clicks, like a label or a background
  class TestPassiveWindow final : public UI::BaseWindow
  {
  public:
    explicit TestPassiveWindow(const std::shared_ptr<UI::BaseWindowContext>& context)
      : UI::BaseWindow(context)
    {
      SetAlignmentX(UI::ItemAlignment::Near);
      SetAlignmentY(UI::ItemAlignment::Near);
    }

  protected:
    PxSize2D MeasureOverride(const UI::PxAvailableSize& /*availableSizePx*/) override
    {
      return ContentSizePx;
    }
  };

  class TestControlInputCatcher : public TestFixtureFslBase
  {
  protected:
    std::shared_ptr<DataBinding::DataBindingService> m_dataBindingService;
    UI::UIManager m_manager;
    std::shared_ptr<UI::BaseWindowContext> m_context;

  public:
    TestControlInputCatcher()
      : m_dataBindingService(std::make_shared<DataBinding::DataBindingService>())
      , m_manager(m_dataBindingService, std::make_unique<UI::RenderStub::RenderSystem>(), UI::UIColorSpace::SRGBNonLinear, false,
                  BasicWindowMetrics(PxExtent2D::Create(800, 600), Vector2(160, 160), 160))
      , m_context(std::make_shared<UI::BaseWindowContext>(m_manager.GetUIContext(), 160, UI::UIColorSpace::SRGBNonLinear))
    {
    }

    void AddAndLayout(const std::shared_ptr<UI::BaseWindow>& window)
    {
      m_manager.GetWindowManager()->Add(window);
      m_manager.Update(TimeSpan(0));
    }

    //! A press and its release, returns if the UI handled the press
    bool Click(const PxPoint2 positionPx)
    {
      const bool isHandled = m_manager.SendMouseButtonEvent(MillisecondTickCount32(), positionPx, true, false);
      m_manager.SendMouseButtonEvent(MillisecondTickCount32(), positionPx, false, false);
      return isHandled;
    }
  };
}


TEST_F(TestControlInputCatcher, PassiveWindow_DoesNotTakeAClick)
{
  AddAndLayout(std::make_shared<TestPassiveWindow>(m_context));

  // This is why a app gets a click on a label or a background
  EXPECT_FALSE(Click(OnContentPx));
}


TEST_F(TestControlInputCatcher, ClickOnItsContent_IsHandled)
{
  const auto catcher = std::make_shared<UI::InputCatcher>(m_context);
  catcher->SetAlignmentX(UI::ItemAlignment::Near);
  catcher->SetAlignmentY(UI::ItemAlignment::Near);
  catcher->SetContent(std::make_shared<TestPassiveWindow>(m_context));
  AddAndLayout(catcher);

  EXPECT_TRUE(Click(OnContentPx));
}


TEST_F(TestControlInputCatcher, ClickOutsideItsArea_IsNotHandled)
{
  const auto catcher = std::make_shared<UI::InputCatcher>(m_context);
  catcher->SetAlignmentX(UI::ItemAlignment::Near);
  catcher->SetAlignmentY(UI::ItemAlignment::Near);
  catcher->SetContent(std::make_shared<TestPassiveWindow>(m_context));
  AddAndLayout(catcher);

  EXPECT_FALSE(Click(OutsidePx));
}
