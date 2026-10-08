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
#include <FslBase/Math/Dp/DpThicknessF.hpp>
#include <FslBase/Math/Pixel/PxPoint2.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Time/MillisecondTickCount32.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDataBinding/Base/DataBindingService.hpp>
#include <FslSimpleUI/Base/BaseWindow.hpp>
#include <FslSimpleUI/Base/BaseWindowContext.hpp>
#include <FslSimpleUI/Base/Control/ScrollViewer.hpp>
#include <FslSimpleUI/Base/IWindowManager.hpp>
#include <FslSimpleUI/Base/ItemAlignment.hpp>
#include <FslSimpleUI/Base/Layout/StackLayout.hpp>
#include <FslSimpleUI/Base/PxAvailableSize.hpp>
#include <FslSimpleUI/Base/System/UIManager.hpp>
#include <FslSimpleUI/Render/Stub/RenderSystem.hpp>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace Fsl;

namespace
{
  //! The UI is 800x600 at 160 dpi, where a dp is a pixel
  constexpr int32_t ViewWidthPx = 800;
  constexpr int32_t ViewHeightPx = 600;
  constexpr int32_t RowWidthPx = 200;
  constexpr int32_t RowHeightPx = 100;
  //! Twenty rows are 2000 pixels of content: it scrolls from 0 to -1400
  constexpr std::size_t RowCount = 20;
  //! One notch of the wheel towards the user (scroll down) scrolls 48 dp
  constexpr int32_t WheelDown = -UI::UIManager::MouseWheelDeltaPerNotch;
  constexpr int32_t NotchPx = 48;
  constexpr PxPoint2 OnViewerPx = PxPoint2::Create(100, 300);

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

  class TestControlScrollViewerMakeVisible : public TestFixtureFslBase
  {
  protected:
    std::shared_ptr<DataBinding::DataBindingService> m_dataBindingService;
    UI::UIManager m_manager;
    std::shared_ptr<UI::BaseWindowContext> m_context;
    //! The rows of the viewer CreateViewerOfRows made, and the stack they are in (the content of the viewer)
    std::vector<std::shared_ptr<TestSizedWindow>> m_rows;
    std::shared_ptr<UI::StackLayout> m_stack;

  public:
    TestControlScrollViewerMakeVisible()
      : m_dataBindingService(std::make_shared<DataBinding::DataBindingService>())
      , m_manager(m_dataBindingService, std::make_unique<UI::RenderStub::RenderSystem>(), UI::UIColorSpace::SRGBNonLinear, false,
                  BasicWindowMetrics(PxExtent2D::Create(ViewWidthPx, ViewHeightPx), Vector2(160, 160), 160))
      , m_context(std::make_shared<UI::BaseWindowContext>(m_manager.GetUIContext(), 160, UI::UIColorSpace::SRGBNonLinear))
    {
    }

    std::shared_ptr<TestSizedWindow> CreateSizedWindow(const int32_t widthPx, const int32_t heightPx)
    {
      return std::make_shared<TestSizedWindow>(m_context, PxSize2D::Create(widthPx, heightPx));
    }

    //! A scroll viewer that fills the screen, with a stack of rows as its content
    std::shared_ptr<UI::ScrollViewer> CreateViewerOfRows(const UI::ScrollModeFlags scrollMode, const std::size_t rowCount = RowCount,
                                                         const UI::LayoutOrientation orientation = UI::LayoutOrientation::Vertical)
    {
      m_stack = std::make_shared<UI::StackLayout>(m_context);
      m_stack->SetOrientation(orientation);
      m_stack->SetAlignmentX(UI::ItemAlignment::Near);
      m_stack->SetAlignmentY(UI::ItemAlignment::Near);
      for (std::size_t i = 0; i < rowCount; ++i)
      {
        // Sideways the rows stand next to each other, so they are as wide as a row is high
        m_rows.push_back(orientation == UI::LayoutOrientation::Vertical ? CreateSizedWindow(RowWidthPx, RowHeightPx)
                                                                        : CreateSizedWindow(RowHeightPx, RowWidthPx));
        m_stack->AddChild(m_rows.back());
      }
      auto viewer = std::make_shared<UI::ScrollViewer>(m_context);
      viewer->SetScrollMode(scrollMode);
      viewer->SetContent(m_stack);
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

    //! Where the row is in the view (the viewer fills the screen)
    [[nodiscard]] PxPoint2 RowPx(const std::size_t index) const
    {
      return m_rows[index]->ScreenPositionPx();
    }

    //! A drag past the start of the content that is held still before the release, so it ends without a flick: the content follows the
    //! finger, held back by the spring, and is on its way back to the start (a bounce of a second) when this returns
    void StartBounceAtTheStart()
    {
      const PxPoint2 draggedToPx = OnViewerPx + PxPoint2::Create(0, 200);
      m_manager.SendMouseButtonEvent(MillisecondTickCount32(1000), OnViewerPx, true, false);
      m_manager.SendMouseMoveEvent(MillisecondTickCount32(1200), draggedToPx, false);
      m_manager.Update(TimeSpan::FromMilliseconds(16));
      m_manager.SendMouseMoveEvent(MillisecondTickCount32(1700), draggedToPx, false);
      m_manager.Update(TimeSpan::FromMilliseconds(16));
      m_manager.SendMouseButtonEvent(MillisecondTickCount32(2200), draggedToPx, false, false);
      m_manager.Update(TimeSpan::FromMilliseconds(16));
      m_manager.Update(TimeSpan::FromMilliseconds(16));
    }
  };
}


TEST_F(TestControlScrollViewerMakeVisible, ARowBelowTheView_ItsEndIsAtTheEndOfTheView)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  ASSERT_EQ(PxPoint2::Create(0, 900), RowPx(9));

  viewer->MakeVisible(m_rows[9]);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), RowPx(9));
  // And nothing moves it after that
  m_manager.Update(TimeSpan::FromMilliseconds(16));
  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), RowPx(9));
}


TEST_F(TestControlScrollViewerMakeVisible, ARowAboveTheView_ItsStartIsAtTheStartOfTheView)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  viewer->MakeVisible(m_rows[19]);
  Layout();
  ASSERT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), RowPx(19));
  ASSERT_EQ(PxPoint2::Create(0, -1200), RowPx(2));

  viewer->MakeVisible(m_rows[2]);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, 0), RowPx(2));
}


TEST_F(TestControlScrollViewerMakeVisible, ARowInsideTheView_NothingMoves)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  m_manager.SendMouseWheelEvent(MillisecondTickCount32(), OnViewerPx, WheelDown);
  Layout();
  ASSERT_EQ(PxPoint2::Create(0, 300 - NotchPx), RowPx(3));

  viewer->MakeVisible(m_rows[3]);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, 300 - NotchPx), RowPx(3));
}


TEST_F(TestControlScrollViewerMakeVisible, ARowThatIsPartlyInTheView_IsBroughtInByThePartThatWasOutside)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  m_manager.SendMouseWheelEvent(MillisecondTickCount32(), OnViewerPx, WheelDown);
  Layout();
  // The first row is partly above the view and the seventh partly below it
  ASSERT_EQ(PxPoint2::Create(0, -NotchPx), RowPx(0));
  ASSERT_EQ(PxPoint2::Create(0, 600 - NotchPx), RowPx(6));

  viewer->MakeVisible(m_rows[6]);
  Layout();
  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), RowPx(6));

  viewer->MakeVisible(m_rows[0]);
  Layout();
  EXPECT_EQ(PxPoint2::Create(0, 0), RowPx(0));
}


TEST_F(TestControlScrollViewerMakeVisible, TheLastRow_TheContentIsAtItsEnd)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);

  viewer->MakeVisible(m_rows[RowCount - 1]);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - (static_cast<int32_t>(RowCount) * RowHeightPx)), RowPx(0));
}


TEST_F(TestControlScrollViewerMakeVisible, AWindowLargerThanTheView_IsShownFromItsStart)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  // Below the twenty rows, and below it one more row so the content does not end with it
  const auto tallWindow = CreateSizedWindow(RowWidthPx, 900);
  m_stack->AddChild(tallWindow);
  m_stack->AddChild(CreateSizedWindow(RowWidthPx, RowHeightPx));
  AddAndLayout(viewer);
  viewer->MakeVisible(m_rows[9]);
  Layout();
  ASSERT_EQ(PxPoint2::Create(0, 2000 - 400), tallWindow->ScreenPositionPx());

  viewer->MakeVisible(tallWindow);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, 0), tallWindow->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerMakeVisible, AWindowInsideAChildOfTheContent)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  // A stack below the rows, the window is the second one in it
  const auto innerStack = std::make_shared<UI::StackLayout>(m_context);
  innerStack->SetOrientation(UI::LayoutOrientation::Vertical);
  innerStack->AddChild(CreateSizedWindow(RowWidthPx, RowHeightPx));
  const auto innerWindow = CreateSizedWindow(RowWidthPx, RowHeightPx);
  innerStack->AddChild(innerWindow);
  innerStack->AddChild(CreateSizedWindow(RowWidthPx, RowHeightPx));
  m_stack->AddChild(innerStack);
  AddAndLayout(viewer);
  ASSERT_EQ(PxPoint2::Create(0, 2100), innerWindow->ScreenPositionPx());

  viewer->MakeVisible(innerWindow);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), innerWindow->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerMakeVisible, TheContentItself_IsShownFromItsStart)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  viewer->MakeVisible(m_rows[9]);
  Layout();
  ASSERT_EQ(PxPoint2::Create(0, -400), RowPx(0));

  viewer->MakeVisible(m_stack);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, 0), RowPx(0));
}


TEST_F(TestControlScrollViewerMakeVisible, AskedBeforeTheFirstLayout_TheFirstLayoutScrolls)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  viewer->MakeVisible(m_rows[9]);

  AddAndLayout(viewer);

  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), RowPx(9));
}


TEST_F(TestControlScrollViewerMakeVisible, TwoCallsBeforeALayout_TheLastOneCounts)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);

  viewer->MakeVisible(m_rows[15]);
  viewer->MakeVisible(m_rows[9]);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), RowPx(9));
}


TEST_F(TestControlScrollViewerMakeVisible, ARowThatWasAddedInTheSameFrame_IsScrolledToWhereThisLayoutPutIt)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  const auto newRow = CreateSizedWindow(RowWidthPx, RowHeightPx);

  m_stack->AddChild(newRow);
  viewer->MakeVisible(newRow);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), newRow->ScreenPositionPx());
}


TEST_F(TestControlScrollViewerMakeVisible, ContentThatFits_NothingMoves)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY, 3);
  AddAndLayout(viewer);

  viewer->MakeVisible(m_rows[2]);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, 0), RowPx(0));
}


TEST_F(TestControlScrollViewerMakeVisible, AWindowThatIsNotInTheContent_IsIgnored)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  viewer->MakeVisible(m_rows[9]);
  Layout();
  const auto otherWindow = CreateSizedWindow(RowWidthPx, RowHeightPx);
  AddAndLayout(otherWindow);
  const auto windowThatIsNowhere = CreateSizedWindow(RowWidthPx, RowHeightPx);

  viewer->MakeVisible(otherWindow);
  Layout();
  EXPECT_EQ(PxPoint2::Create(0, -400), RowPx(0));

  viewer->MakeVisible(windowThatIsNowhere);
  Layout();
  EXPECT_EQ(PxPoint2::Create(0, -400), RowPx(0));

  // The viewer is not in its own content
  viewer->MakeVisible(viewer);
  Layout();
  EXPECT_EQ(PxPoint2::Create(0, -400), RowPx(0));
}


TEST_F(TestControlScrollViewerMakeVisible, AWindowThatIsGoneAtTheLayout_IsIgnored)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  {
    const auto shortLivedWindow = CreateSizedWindow(RowWidthPx, RowHeightPx);
    viewer->MakeVisible(shortLivedWindow);
  }

  Layout();

  EXPECT_EQ(PxPoint2::Create(0, 0), RowPx(0));
}


TEST_F(TestControlScrollViewerMakeVisible, NoWindow_Throws)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);

  EXPECT_THROW(viewer->MakeVisible(std::shared_ptr<UI::BaseWindow>()), std::invalid_argument);
}


TEST_F(TestControlScrollViewerMakeVisible, AWheelEventInTheSameFrame_TheWindowIsInTheView)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);

  m_manager.SendMouseWheelEvent(MillisecondTickCount32(), OnViewerPx, WheelDown);
  viewer->MakeVisible(m_rows[9]);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), RowPx(9));
}


TEST_F(TestControlScrollViewerMakeVisible, AfterTheWheel_ARowThatWasScrolledAwayComesBack)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  for (int i = 0; i < 10; ++i)
  {
    m_manager.SendMouseWheelEvent(MillisecondTickCount32(), OnViewerPx, WheelDown);
    Layout();
  }
  ASSERT_EQ(PxPoint2::Create(0, -10 * NotchPx), RowPx(0));

  viewer->MakeVisible(m_rows[1]);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, 0), RowPx(1));
}


TEST_F(TestControlScrollViewerMakeVisible, SidewaysViewer)
{
  // Rows of 100x200 next to each other
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateX, RowCount, UI::LayoutOrientation::Horizontal);
  AddAndLayout(viewer);
  ASSERT_EQ(PxPoint2::Create(1200, 0), RowPx(12));

  viewer->MakeVisible(m_rows[12]);
  Layout();
  EXPECT_EQ(PxPoint2::Create(ViewWidthPx - RowHeightPx, 0), RowPx(12));

  viewer->MakeVisible(m_rows[1]);
  Layout();
  EXPECT_EQ(PxPoint2::Create(0, 0), RowPx(1));
}


TEST_F(TestControlScrollViewerMakeVisible, ViewerThatScrollsBothWays_AAxisTheWindowIsInsideOnKeepsItsPlace)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::Translate);
  // Every row begins 1300 pixels from the left, so the content is 1500 pixels wide
  for (const auto& row : m_rows)
  {
    row->SetMargin(DpThicknessF::Create(1300, 0, 0, 0));
  }
  AddAndLayout(viewer);
  ASSERT_EQ(PxPoint2::Create(1300, 0), RowPx(0));

  // Outside the view sideways only
  viewer->MakeVisible(m_rows[0]);
  Layout();
  ASSERT_EQ(PxPoint2::Create(ViewWidthPx - RowWidthPx, 0), RowPx(0));

  // Outside the view vertically only
  viewer->MakeVisible(m_rows[9]);
  Layout();
  EXPECT_EQ(PxPoint2::Create(ViewWidthPx - RowWidthPx, ViewHeightPx - RowHeightPx), RowPx(9));
}


TEST_F(TestControlScrollViewerMakeVisible, ViewerThatScrollsBothWays_BothAxesMove)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::Translate);
  for (const auto& row : m_rows)
  {
    row->SetMargin(DpThicknessF::Create(1300, 0, 0, 0));
  }
  AddAndLayout(viewer);

  viewer->MakeVisible(m_rows[9]);
  Layout();

  EXPECT_EQ(PxPoint2::Create(ViewWidthPx - RowWidthPx, ViewHeightPx - RowHeightPx), RowPx(9));
}


TEST_F(TestControlScrollViewerMakeVisible, DuringABounce_TheBounceEndsAndTheWindowIsInTheView)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  StartBounceAtTheStart();
  ASSERT_GT(RowPx(0).Y.Value, 0);

  viewer->MakeVisible(m_rows[9]);
  Layout();

  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), RowPx(9));
  m_manager.Update(TimeSpan::FromMilliseconds(16));
  m_manager.Update(TimeSpan::FromMilliseconds(500));
  EXPECT_EQ(PxPoint2::Create(0, ViewHeightPx - RowHeightPx), RowPx(9));
}


TEST_F(TestControlScrollViewerMakeVisible, DuringABounce_AWindowThatIsInTheViewLeavesTheBounceAlone)
{
  const auto viewer = CreateViewerOfRows(UI::ScrollModeFlags::TranslateY);
  AddAndLayout(viewer);
  StartBounceAtTheStart();
  const int32_t duringTheBouncePx = RowPx(0).Y.Value;
  ASSERT_GT(duringTheBouncePx, 0);

  viewer->MakeVisible(m_rows[1]);
  m_manager.Update(TimeSpan::FromMilliseconds(300));

  // It went on towards the start, and was not put there
  EXPECT_GT(RowPx(0).Y.Value, 0);
  EXPECT_LT(RowPx(0).Y.Value, duringTheBouncePx);
}
