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
#include <FslBase/Time/MillisecondTickCount32.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDataBinding/Base/DataBindingService.hpp>
#include <FslSimpleUI/Base/BaseWindow.hpp>
#include <FslSimpleUI/Base/BaseWindowContext.hpp>
#include <FslSimpleUI/Base/Event/EventTransactionState.hpp>
#include <FslSimpleUI/Base/Event/WindowInputClickEvent.hpp>
#include <FslSimpleUI/Base/Event/WindowMouseOverEvent.hpp>
#include <FslSimpleUI/Base/IWindowManager.hpp>
#include <FslSimpleUI/Base/ItemAlignment.hpp>
#include <FslSimpleUI/Base/PxAvailableSize.hpp>
#include <FslSimpleUI/Base/System/UIManager.hpp>
#include <FslSimpleUI/Base/WindowFlags.hpp>
#include <FslSimpleUI/Render/Stub/RenderSystem.hpp>
#include <memory>
#include <vector>

using namespace Fsl;

namespace
{
  constexpr PxSize2D ContentSizePx = PxSize2D::Create(200, 100);
  //! A point on the window, which is laid out in the top left corner
  constexpr PxPoint2 OnWindowPx = PxPoint2::Create(50, 50);
  constexpr PxPoint2 AlsoOnWindowPx = PxPoint2::Create(60, 50);
  //! A point outside the window
  constexpr PxPoint2 OutsidePx = PxPoint2::Create(500, 400);

  //! A window of 200x100 that takes clicks and the mouse over, and keeps what it was sent (the repeats left out)
  class TestInputWindow final : public UI::BaseWindow
  {
  public:
    std::vector<UI::EventTransactionState> Clicks;
    std::vector<UI::EventTransactionState> MouseOvers;

    explicit TestInputWindow(const std::shared_ptr<UI::BaseWindowContext>& context)
      : UI::BaseWindow(context)
    {
      SetAlignmentX(UI::ItemAlignment::Near);
      SetAlignmentY(UI::ItemAlignment::Near);
      Enable(UI::WindowFlags(UI::WindowFlags::ClickInput | UI::WindowFlags::MouseOver));
    }

  protected:
    void OnClickInput(const std::shared_ptr<UI::WindowInputClickEvent>& theEvent) override
    {
      if (!theEvent->IsRepeat())
      {
        Clicks.push_back(theEvent->GetState());
      }
      theEvent->Handled();
    }

    void OnMouseOver(const std::shared_ptr<UI::WindowMouseOverEvent>& theEvent) override
    {
      if (!theEvent->IsRepeat())
      {
        MouseOvers.push_back(theEvent->GetState());
      }
      theEvent->Handled();
    }

    PxSize2D MeasureOverride(const UI::PxAvailableSize& /*availableSizePx*/) override
    {
      return ContentSizePx;
    }
  };

  class TestUIManagerInputCancel : public TestFixtureFslBase
  {
  protected:
    std::shared_ptr<DataBinding::DataBindingService> m_dataBindingService;
    UI::UIManager m_manager;
    std::shared_ptr<UI::BaseWindowContext> m_context;
    std::shared_ptr<TestInputWindow> m_window;

  public:
    TestUIManagerInputCancel()
      : m_dataBindingService(std::make_shared<DataBinding::DataBindingService>())
      , m_manager(m_dataBindingService, std::make_unique<UI::RenderStub::RenderSystem>(), UI::UIColorSpace::SRGBNonLinear, false,
                  BasicWindowMetrics(PxExtent2D::Create(800, 600), Vector2(160, 160), 160))
      , m_context(std::make_shared<UI::BaseWindowContext>(m_manager.GetUIContext(), 160, UI::UIColorSpace::SRGBNonLinear))
      , m_window(std::make_shared<TestInputWindow>(m_context))
    {
      m_manager.GetWindowManager()->Add(m_window);
      m_manager.Update(TimeSpan(0));
    }

    bool Press(const PxPoint2 positionPx)
    {
      return m_manager.SendMouseButtonEvent(MillisecondTickCount32(), positionPx, true, false);
    }

    bool Release(const PxPoint2 positionPx)
    {
      return m_manager.SendMouseButtonEvent(MillisecondTickCount32(), positionPx, false, false);
    }

    bool Move(const PxPoint2 positionPx)
    {
      return m_manager.SendMouseMoveEvent(MillisecondTickCount32(), positionPx, false);
    }
  };

  using States = std::vector<UI::EventTransactionState>;
}


TEST_F(TestUIManagerInputCancel, NothingBegun_NothingIsCanceled)
{
  EXPECT_FALSE(m_manager.SendInputCancelEvent());

  EXPECT_TRUE(m_window->Clicks.empty());
  EXPECT_TRUE(m_window->MouseOvers.empty());
}


TEST_F(TestUIManagerInputCancel, PressAndRelease_IsAClick)
{
  // The case a cancel is told apart from
  EXPECT_TRUE(Press(OnWindowPx));
  Release(OnWindowPx);

  EXPECT_EQ((States{UI::EventTransactionState::Begin, UI::EventTransactionState::End}), m_window->Clicks);
}


TEST_F(TestUIManagerInputCancel, PressThenCancel_TheWindowGetsACancelAndNoEnd)
{
  Press(OnWindowPx);

  EXPECT_TRUE(m_manager.SendInputCancelEvent());

  EXPECT_EQ((States{UI::EventTransactionState::Begin, UI::EventTransactionState::Canceled}), m_window->Clicks);
}


TEST_F(TestUIManagerInputCancel, ReleaseAfterACancel_EndsNothing)
{
  Press(OnWindowPx);
  m_manager.SendInputCancelEvent();

  // The button was still held when the window lost the focus, and is let go later
  EXPECT_FALSE(Release(OnWindowPx));

  EXPECT_EQ((States{UI::EventTransactionState::Begin, UI::EventTransactionState::Canceled}), m_window->Clicks);
}


TEST_F(TestUIManagerInputCancel, MoveAfterACancel_IsNoDrag)
{
  Press(OnWindowPx);
  m_manager.SendInputCancelEvent();
  m_window->Clicks.clear();

  // The button is still held, the UI no longer counts it as held
  Move(AlsoOnWindowPx);

  EXPECT_TRUE(m_window->Clicks.empty());
}


TEST_F(TestUIManagerInputCancel, ClickAfterACancel_IsAClickAgain)
{
  Press(OnWindowPx);
  m_manager.SendInputCancelEvent();
  Release(OnWindowPx);
  m_window->Clicks.clear();

  EXPECT_TRUE(Press(OnWindowPx));
  Release(OnWindowPx);

  EXPECT_EQ((States{UI::EventTransactionState::Begin, UI::EventTransactionState::End}), m_window->Clicks);
}


TEST_F(TestUIManagerInputCancel, PressAfterACancelWithoutTheRelease_IsAClickAgain)
{
  // The release went to another window of the system and never came
  Press(OnWindowPx);
  m_manager.SendInputCancelEvent();
  m_window->Clicks.clear();

  EXPECT_TRUE(Press(OnWindowPx));
  Release(OnWindowPx);

  EXPECT_EQ((States{UI::EventTransactionState::Begin, UI::EventTransactionState::End}), m_window->Clicks);
}


TEST_F(TestUIManagerInputCancel, PressOutsideEveryWindowThenCancel_TheNextClickWorks)
{
  // A click no window took is begun as well, and has to be over before the next one
  EXPECT_FALSE(Press(OutsidePx));
  m_manager.SendInputCancelEvent();
  Release(OutsidePx);

  EXPECT_TRUE(Press(OnWindowPx));
  Release(OnWindowPx);

  EXPECT_EQ((States{UI::EventTransactionState::Begin, UI::EventTransactionState::End}), m_window->Clicks);
}


TEST_F(TestUIManagerInputCancel, MouseOverThenCancel_TheWindowGetsACancel)
{
  Move(OnWindowPx);
  ASSERT_EQ((States{UI::EventTransactionState::Begin}), m_window->MouseOvers);

  EXPECT_FALSE(m_manager.SendInputCancelEvent());

  EXPECT_EQ((States{UI::EventTransactionState::Begin, UI::EventTransactionState::Canceled}), m_window->MouseOvers);
}


TEST_F(TestUIManagerInputCancel, MoveAfterAMouseOverCancel_BeginsTheMouseOverAgain)
{
  Move(OnWindowPx);
  m_manager.SendInputCancelEvent();
  m_window->MouseOvers.clear();

  // The pointer is still over the window when the focus is back
  Move(AlsoOnWindowPx);

  EXPECT_EQ((States{UI::EventTransactionState::Begin}), m_window->MouseOvers);
}


TEST_F(TestUIManagerInputCancel, CancelTwice_TheSecondCancelsNothing)
{
  Press(OnWindowPx);
  EXPECT_TRUE(m_manager.SendInputCancelEvent());
  EXPECT_FALSE(m_manager.SendInputCancelEvent());

  EXPECT_EQ((States{UI::EventTransactionState::Begin, UI::EventTransactionState::Canceled}), m_window->Clicks);
}
