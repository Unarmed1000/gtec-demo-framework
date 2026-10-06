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
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Time/MillisecondTickCount32.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDataBinding/Base/DataBindingService.hpp>
#include <FslSimpleUI/Base/BaseWindow.hpp>
#include <FslSimpleUI/Base/BaseWindowContext.hpp>
#include <FslSimpleUI/Base/Control/InputCatcher.hpp>
#include <FslSimpleUI/Base/Control/ScrollViewer.hpp>
#include <FslSimpleUI/Base/IWindowManager.hpp>
#include <FslSimpleUI/Base/ItemAlignment.hpp>
#include <FslSimpleUI/Base/ItemVisibility.hpp>
#include <FslSimpleUI/Base/Layout/StackLayout.hpp>
#include <FslSimpleUI/Base/PxAvailableSize.hpp>
#include <FslSimpleUI/Base/System/UIManager.hpp>
#include <FslSimpleUI/Render/Stub/RenderSystem.hpp>
#include <memory>

using namespace Fsl;

namespace
{
  //! The UI is 800x600 at 160 dpi, where a dp is a pixel: a notch of the wheel scrolls 48 dp, so 48 pixels
  constexpr int32_t NotchPx = 48;
  constexpr int32_t ViewWidthPx = 800;
  constexpr int32_t ViewHeightPx = 600;
  //! One notch towards the user (scroll down) and one away from the user (scroll up)
  constexpr int32_t WheelDown = -UI::UIManager::MouseWheelDeltaPerNotch;
  constexpr int32_t WheelUp = UI::UIManager::MouseWheelDeltaPerNotch;

  //! A window of a fixed size that takes no input, like a label or a picture
  class TestSizedWindow final : public UI::BaseWindow
  {
    PxSize2D m_sizePx;

  public:
    TestSizedWindow(const std::shared_ptr<UI::BaseWindowContext>& context, const PxSize2D sizePx)
      : UI::BaseWindow(context)
      , m_sizePx(sizePx)
    {
      SetAlignmentX(UI::ItemAlignment::Near);
      SetAlignmentY(UI::ItemAlignment::Near);
    }

    //! Where the top left corner of the window is on the screen
    [[nodiscard]] PxPoint2 ScreenPositionPx() const
    {
      return PointToScreen(PxPoint2());
    }

  protected:
    PxSize2D MeasureOverride(const UI::PxAvailableSize& /*availableSizePx*/) override
    {
      return m_sizePx;
    }
  };

  class TestControlScrollViewerWheel : public TestFixtureFslBase
  {
  protected:
    std::shared_ptr<DataBinding::DataBindingService> m_dataBindingService;
    UI::UIManager m_manager;
    std::shared_ptr<UI::BaseWindowContext> m_context;

  public:
    TestControlScrollViewerWheel()
      : m_dataBindingService(std::make_shared<DataBinding::DataBindingService>())
      , m_manager(m_dataBindingService, std::make_unique<UI::RenderStub::RenderSystem>(), UI::UIColorSpace::SRGBNonLinear, false,
                  BasicWindowMetrics(PxExtent2D::Create(ViewWidthPx, ViewHeightPx), Vector2(160, 160), 160))
      , m_context(std::make_shared<UI::BaseWindowContext>(m_manager.GetUIContext(), 160, UI::UIColorSpace::SRGBNonLinear))
    {
    }

    std::shared_ptr<TestSizedWindow> CreateContent(const int32_t widthPx, const int32_t heightPx)
    {
      return std::make_shared<TestSizedWindow>(m_context, PxSize2D::Create(widthPx, heightPx));
    }

    //! A scroll viewer that fills the screen
    std::shared_ptr<UI::ScrollViewer> CreateViewer(const UI::ScrollModeFlags scrollMode, const std::shared_ptr<UI::BaseWindow>& content)
    {
      auto viewer = std::make_shared<UI::ScrollViewer>(m_context);
      viewer->SetScrollMode(scrollMode);
      viewer->SetContent(content);
      return viewer;
    }

    void AddAndLayout(const std::shared_ptr<UI::BaseWindow>& window)
    {
      m_manager.GetWindowManager()->Add(window);
      Layout();
    }

    void Layout()
    {
      m_manager.Update(TimeSpan(0));
    }

    //! Turns the wheel at a position and lays the UI out, returns if the UI handled the event
    bool Wheel(const PxPoint2 positionPx, const int32_t delta)
    {
      const bool isHandled = m_manager.SendMouseWheelEvent(MillisecondTickCount32(), positionPx, delta);
      Layout();
      return isHandled;
    }
  };

  constexpr PxPoint2 OnViewerPx = PxPoint2::Create(100, 300);
}


TEST_F(TestControlScrollViewerWheel, WheelDown_ScrollsTheContentUpByANotch_AndIsHandled)
{
  const auto content = CreateContent(200, 2000);
  AddAndLayout(CreateViewer(UI::ScrollModeFlags::TranslateY, content));
  ASSERT_EQ(PxPoint2::Create(0, 0), content->ScreenPositionPx());

  EXPECT_TRUE(Wheel(OnViewerPx, WheelDown));

  EXPECT_EQ(PxPoint2::Create(0, -NotchPx), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, WheelUp_ScrollsBack)
{
  const auto content = CreateContent(200, 2000);
  AddAndLayout(CreateViewer(UI::ScrollModeFlags::TranslateY, content));
  Wheel(OnViewerPx, WheelDown);
  Wheel(OnViewerPx, WheelDown);

  EXPECT_TRUE(Wheel(OnViewerPx, WheelUp));

  EXPECT_EQ(PxPoint2::Create(0, -NotchPx), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, PartsOfANotch_ScrollTheirPart)
{
  // A touchpad, or a wheel with fine steps
  const auto content = CreateContent(200, 2000);
  AddAndLayout(CreateViewer(UI::ScrollModeFlags::TranslateY, content));

  EXPECT_TRUE(Wheel(OnViewerPx, WheelDown / 4));

  EXPECT_EQ(PxPoint2::Create(0, -NotchPx / 4), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, WheelUpAtTheStart_IsHandledAndDoesNotMove)
{
  // No overscroll with the wheel. The viewer still takes the event: a viewer around it must not scroll in its place.
  const auto content = CreateContent(200, 2000);
  AddAndLayout(CreateViewer(UI::ScrollModeFlags::TranslateY, content));

  EXPECT_TRUE(Wheel(OnViewerPx, WheelUp));

  EXPECT_EQ(PxPoint2::Create(0, 0), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, WheelDown_StopsAtTheEndOfTheContent)
{
  const auto content = CreateContent(200, 2000);
  AddAndLayout(CreateViewer(UI::ScrollModeFlags::TranslateY, content));

  for (int i = 0; i < 100; ++i)
  {
    Wheel(OnViewerPx, WheelDown);
  }

  // The last line of the content is the last line of the view
  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - 2000), content->ScreenPositionPx());
  // And from there it scrolls back by a notch, so nothing was kept of what did not fit
  Wheel(OnViewerPx, WheelUp);
  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - 2000 + NotchPx), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, TwoWheelEventsBeforeALayout_AddUp)
{
  const auto content = CreateContent(200, 2000);
  AddAndLayout(CreateViewer(UI::ScrollModeFlags::TranslateY, content));

  EXPECT_TRUE(m_manager.SendMouseWheelEvent(MillisecondTickCount32(), OnViewerPx, WheelDown));
  EXPECT_TRUE(m_manager.SendMouseWheelEvent(MillisecondTickCount32(), OnViewerPx, WheelDown));
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, -2 * NotchPx), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, ContentThatFits_DoesNotTakeTheWheel)
{
  // Nothing to scroll, so the wheel is left to whatever is behind the viewer (the app)
  const auto content = CreateContent(200, 100);
  AddAndLayout(CreateViewer(UI::ScrollModeFlags::TranslateY, content));

  EXPECT_FALSE(Wheel(OnViewerPx, WheelDown));

  EXPECT_EQ(PxPoint2::Create(0, 0), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, WheelOutsideTheViewer_IsNotHandled)
{
  const auto content = CreateContent(200, 2000);
  const auto viewer = CreateViewer(UI::ScrollModeFlags::TranslateY, content);
  // The viewer is as wide as its content, at the left edge
  viewer->SetAlignmentX(UI::ItemAlignment::Near);
  AddAndLayout(viewer);

  EXPECT_FALSE(Wheel(PxPoint2::Create(500, 300), WheelDown));

  EXPECT_EQ(PxPoint2::Create(0, 0), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, HiddenViewer_DoesNotTakeTheWheel)
{
  const auto content = CreateContent(200, 2000);
  const auto viewer = CreateViewer(UI::ScrollModeFlags::TranslateY, content);
  AddAndLayout(viewer);
  viewer->SetVisibility(UI::ItemVisibility::Hidden);
  Layout();

  EXPECT_FALSE(Wheel(OnViewerPx, WheelDown));
}


TEST_F(TestControlScrollViewerWheel, SidewaysOnlyViewer_ScrollsSidewaysWithTheWheel)
{
  const auto content = CreateContent(2000, 100);
  AddAndLayout(CreateViewer(UI::ScrollModeFlags::TranslateX, content));

  EXPECT_TRUE(Wheel(PxPoint2::Create(100, 50), WheelDown));

  EXPECT_EQ(PxPoint2::Create(-NotchPx, 0), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, ViewerThatScrollsBothWays_ScrollsVerticallyAndKeepsItsHorizontalPosition)
{
  const auto content = CreateContent(2000, 2000);
  AddAndLayout(CreateViewer(UI::ScrollModeFlags::Translate, content));

  EXPECT_TRUE(Wheel(OnViewerPx, WheelDown));

  EXPECT_EQ(PxPoint2::Create(0, -NotchPx), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, NestedViewers_TheInnerOneTakesTheWheel)
{
  // A sideways viewer at the top of the content of a vertical one, with a tall window below it
  const auto innerContent = CreateContent(2000, 100);
  const auto innerViewer = CreateViewer(UI::ScrollModeFlags::TranslateX, innerContent);
  innerViewer->SetAlignmentY(UI::ItemAlignment::Near);
  const auto tallWindow = CreateContent(200, 2000);
  const auto stack = std::make_shared<UI::StackLayout>(m_context);
  stack->SetOrientation(UI::LayoutOrientation::Vertical);
  stack->SetAlignmentX(UI::ItemAlignment::Stretch);
  stack->AddChild(innerViewer);
  stack->AddChild(tallWindow);
  AddAndLayout(CreateViewer(UI::ScrollModeFlags::TranslateY, stack));
  ASSERT_EQ(PxPoint2::Create(0, 100), tallWindow->ScreenPositionPx());

  // Over the inner viewer: it scrolls sideways and the outer one does not move
  EXPECT_TRUE(Wheel(PxPoint2::Create(100, 50), WheelDown));
  EXPECT_EQ(PxPoint2::Create(-NotchPx, 0), innerContent->ScreenPositionPx());
  EXPECT_EQ(PxPoint2::Create(0, 100), tallWindow->ScreenPositionPx());

  // Below it: the outer one scrolls, and takes the inner viewer with it
  EXPECT_TRUE(Wheel(PxPoint2::Create(100, 300), WheelDown));
  EXPECT_EQ(PxPoint2::Create(0, 100 - NotchPx), tallWindow->ScreenPositionPx());
  EXPECT_EQ(PxPoint2::Create(-NotchPx, -NotchPx), innerContent->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, InputCatcher_TakesTheWheelOverItsContent)
{
  const auto catcher = std::make_shared<UI::InputCatcher>(m_context);
  catcher->SetAlignmentX(UI::ItemAlignment::Near);
  catcher->SetAlignmentY(UI::ItemAlignment::Near);
  catcher->SetContent(CreateContent(200, 100));
  AddAndLayout(catcher);

  EXPECT_TRUE(Wheel(PxPoint2::Create(50, 50), WheelDown));
  EXPECT_FALSE(Wheel(PxPoint2::Create(500, 400), WheelDown));
}


TEST_F(TestControlScrollViewerWheel, InputCatcherAroundAViewer_TheViewerScrolls)
{
  // The catcher gets the event after its content, so the viewer in it scrolls as it does without the catcher
  const auto content = CreateContent(200, 2000);
  const auto catcher = std::make_shared<UI::InputCatcher>(m_context);
  catcher->SetContent(CreateViewer(UI::ScrollModeFlags::TranslateY, content));
  AddAndLayout(catcher);

  EXPECT_TRUE(Wheel(OnViewerPx, WheelDown));

  EXPECT_EQ(PxPoint2::Create(0, -NotchPx), content->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerWheel, PassiveWindow_DoesNotTakeTheWheel)
{
  AddAndLayout(CreateContent(200, 100));

  EXPECT_FALSE(Wheel(PxPoint2::Create(50, 50), WheelDown));
}


TEST_F(TestControlScrollViewerWheel, ClipContentSetWhileShown_ContentOutsideTheViewerTakesNoInput)
{
  // A viewer of 200x100 pixels at the top left, and in it a input catcher that is 2000 pixels high
  const auto catcher = std::make_shared<UI::InputCatcher>(m_context);
  catcher->SetContent(CreateContent(200, 2000));
  const auto viewer = CreateViewer(UI::ScrollModeFlags::TranslateY, catcher);
  viewer->SetAlignmentX(UI::ItemAlignment::Near);
  viewer->SetAlignmentY(UI::ItemAlignment::Near);
  viewer->SetHeight(UI::DpLayoutSize1D::Create(100.0f));
  AddAndLayout(viewer);
  const PxPoint2 belowTheViewerPx = PxPoint2::Create(50, 300);

  // Content that is not clipped takes input where it is, also outside its viewer
  EXPECT_TRUE(Wheel(belowTheViewerPx, WheelUp));

  viewer->SetClipContent(true);
  Layout();
  EXPECT_FALSE(Wheel(belowTheViewerPx, WheelUp));
  EXPECT_TRUE(Wheel(PxPoint2::Create(50, 50), WheelUp));

  viewer->SetClipContent(false);
  Layout();
  EXPECT_TRUE(Wheel(belowTheViewerPx, WheelUp));
}


TEST_F(TestControlScrollViewerWheel, FinishAnimation_SettlesABounce)
{
  const auto content = CreateContent(200, 2000);
  const auto viewer = CreateViewer(UI::ScrollModeFlags::TranslateY, content);
  AddAndLayout(viewer);
  // A drag past the start of the content that is held still before the release, so it ends without a flick: the content follows the
  // finger, held back by the spring, and bounces back to the start when it is let go
  const PxPoint2 draggedToPx = OnViewerPx + PxPoint2::Create(0, 200);
  m_manager.SendMouseButtonEvent(MillisecondTickCount32(1000), OnViewerPx, true, false);
  m_manager.SendMouseMoveEvent(MillisecondTickCount32(1200), draggedToPx, false);
  m_manager.Update(TimeSpan::FromMilliseconds(16));
  m_manager.SendMouseMoveEvent(MillisecondTickCount32(1700), draggedToPx, false);
  m_manager.Update(TimeSpan::FromMilliseconds(16));
  ASSERT_GT(content->ScreenPositionPx().Y.Value, 0);
  m_manager.SendMouseButtonEvent(MillisecondTickCount32(2200), draggedToPx, false, false);
  m_manager.Update(TimeSpan::FromMilliseconds(16));
  m_manager.Update(TimeSpan::FromMilliseconds(16));
  // The bounce takes a second, so the content is still past the start
  ASSERT_GT(content->ScreenPositionPx().Y.Value, 0);

  viewer->FinishAnimation();
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, 0), content->ScreenPositionPx());
  // And nothing moves it after that
  m_manager.Update(TimeSpan::FromMilliseconds(16));
  EXPECT_EQ(PxPoint2::Create(0, 0), content->ScreenPositionPx());
}
