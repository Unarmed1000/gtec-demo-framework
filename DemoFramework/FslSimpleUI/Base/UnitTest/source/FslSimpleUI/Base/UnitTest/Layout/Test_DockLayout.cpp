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
#include <FslBase/Math/Pixel/PxRectangle.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslBase/UnitTest/Helper/Common.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDataBinding/Base/DataBindingService.hpp>
#include <FslSimpleUI/Base/BaseWindow.hpp>
#include <FslSimpleUI/Base/BaseWindowContext.hpp>
#include <FslSimpleUI/Base/IWindowManager.hpp>
#include <FslSimpleUI/Base/ItemAlignment.hpp>
#include <FslSimpleUI/Base/Layout/DockLayout.hpp>
#include <FslSimpleUI/Base/PxAvailableSize.hpp>
#include <FslSimpleUI/Base/System/UIManager.hpp>
#include <FslSimpleUI/Render/Stub/RenderSystem.hpp>
#include <memory>

using namespace Fsl;

namespace
{
  //! The UI is 800x600 at 160 dpi, where a dp is a pixel. The layout fills it.
  constexpr int32_t AreaWidthPx = 800;
  constexpr int32_t AreaHeightPx = 600;

  //! A window that asks for a fixed size and fills the area it is given, so its rectangle is the area the layout gave it
  class TestSizedWindow final : public UI::BaseWindow
  {
    PxSize2D m_sizePx;

  public:
    TestSizedWindow(const std::shared_ptr<UI::BaseWindowContext>& context, const PxSize2D sizePx)
      : UI::BaseWindow(context)
      , m_sizePx(sizePx)
    {
      SetAlignmentX(UI::ItemAlignment::Stretch);
      SetAlignmentY(UI::ItemAlignment::Stretch);
    }

    //! The area the layout gave the window, relative to the layout
    [[nodiscard]] PxRectangle RectPx() const
    {
      return WinGetContentRectanglePx();
    }

  protected:
    PxSize2D MeasureOverride(const UI::PxAvailableSize& /*availableSizePx*/) override
    {
      return m_sizePx;
    }
  };

  class TestLayoutDockLayout : public TestFixtureFslBase
  {
  protected:
    std::shared_ptr<DataBinding::DataBindingService> m_dataBindingService;
    UI::UIManager m_manager;
    std::shared_ptr<UI::BaseWindowContext> m_context;
    std::shared_ptr<UI::DockLayout> m_layout;

  public:
    TestLayoutDockLayout()
      : m_dataBindingService(std::make_shared<DataBinding::DataBindingService>())
      , m_manager(m_dataBindingService, std::make_unique<UI::RenderStub::RenderSystem>(), UI::UIColorSpace::SRGBNonLinear, false,
                  BasicWindowMetrics(PxExtent2D::Create(AreaWidthPx, AreaHeightPx), Vector2(160, 160), 160))
      , m_context(std::make_shared<UI::BaseWindowContext>(m_manager.GetUIContext(), 160, UI::UIColorSpace::SRGBNonLinear))
      , m_layout(std::make_shared<UI::DockLayout>(m_context))
    {
      m_layout->SetAlignmentX(UI::ItemAlignment::Stretch);
      m_layout->SetAlignmentY(UI::ItemAlignment::Stretch);
    }

    std::shared_ptr<TestSizedWindow> Child(const int32_t widthPx, const int32_t heightPx)
    {
      return std::make_shared<TestSizedWindow>(m_context, PxSize2D::Create(widthPx, heightPx));
    }

    //! Shows the layout and lays the UI out
    void Layout()
    {
      m_manager.GetWindowManager()->Add(m_layout);
      m_manager.Update(TimeSpan(0));
    }
  };
}


TEST_F(TestLayoutDockLayout, LastChildFill_IsStoredAndReturned)
{
  EXPECT_FALSE(m_layout->GetLastChildFill());
  EXPECT_TRUE(m_layout->SetLastChildFill(true));
  EXPECT_TRUE(m_layout->GetLastChildFill());
  EXPECT_FALSE(m_layout->SetLastChildFill(true));
  EXPECT_TRUE(m_layout->SetLastChildFill(false));
  EXPECT_FALSE(m_layout->GetLastChildFill());
}


TEST_F(TestLayoutDockLayout, LimitToAvailableSpace_IsStoredAndReturned)
{
  EXPECT_FALSE(m_layout->GetLimitToAvailableSpace());
  EXPECT_TRUE(m_layout->SetLimitToAvailableSpace(true));
  EXPECT_TRUE(m_layout->GetLimitToAvailableSpace());
  EXPECT_FALSE(m_layout->SetLimitToAvailableSpace(true));
  EXPECT_TRUE(m_layout->SetLimitToAvailableSpace(false));
  EXPECT_FALSE(m_layout->GetLimitToAvailableSpace());
}


TEST_F(TestLayoutDockLayout, NoChildren_AsksForNothing)
{
  Layout();

  EXPECT_EQ(PxSize2D(), m_layout->DesiredSizePx());
}


TEST_F(TestLayoutDockLayout, Measure_LeftThenTop_EachAddsToItsAxis)
{
  m_layout->AddChild(Child(40, 50), UI::DockType::Left);
  m_layout->AddChild(Child(50, 20), UI::DockType::Top);

  Layout();

  // The child at the left is 40 wide and makes the layout 50 high. The child at the top is next to it: 40 + 50 wide, and 20 high.
  EXPECT_EQ(PxSize2D::Create(90, 50), m_layout->DesiredSizePx());
}


TEST_F(TestLayoutDockLayout, Measure_AChildIsOfferedTheSpaceTheChildrenBeforeItLeft)
{
  // A window that asks for all it is offered
  class TestGreedyWindow final : public UI::BaseWindow
  {
  public:
    explicit TestGreedyWindow(const std::shared_ptr<UI::BaseWindowContext>& context)
      : UI::BaseWindow(context)
    {
    }

  protected:
    PxSize2D MeasureOverride(const UI::PxAvailableSize& availableSizePx) override
    {
      return availableSizePx.ToPxSize2D();
    }
  };
  const auto greedy = std::make_shared<TestGreedyWindow>(m_context);
  m_layout->AddChild(Child(40, 50), UI::DockType::Left);
  m_layout->AddChild(Child(50, 20), UI::DockType::Top);
  m_layout->AddChild(greedy, UI::DockType::Left);

  Layout();

  EXPECT_EQ(PxSize2D::Create(AreaWidthPx - 40, AreaHeightPx - 20), greedy->DesiredSizePx());
}


TEST_F(TestLayoutDockLayout, Arrange_ChildAtTheLeft_IsAsWideAsItAsksForAndAsHighAsTheLayout)
{
  const auto left = Child(40, 50);
  m_layout->AddChild(left, UI::DockType::Left);

  Layout();

  EXPECT_EQ(PxRectangle::Create(0, 0, 40, AreaHeightPx), left->RectPx());
}


TEST_F(TestLayoutDockLayout, Arrange_ChildAddedWithoutAEdge_IsAtTheLeft)
{
  const auto child = Child(40, 50);
  m_layout->AddChild(child);

  Layout();

  EXPECT_EQ(PxRectangle::Create(0, 0, 40, AreaHeightPx), child->RectPx());
}


TEST_F(TestLayoutDockLayout, Arrange_ChildAtTheTop_IsAsWideAsTheLayoutAndAsHighAsItAsksFor)
{
  const auto top = Child(40, 30);
  m_layout->AddChild(top, UI::DockType::Top);

  Layout();

  EXPECT_EQ(PxRectangle::Create(0, 0, AreaWidthPx, 30), top->RectPx());
}


TEST_F(TestLayoutDockLayout, Arrange_ChildAtTheRight_EndsAtTheRightEdge)
{
  const auto right = Child(30, 50);
  m_layout->AddChild(right, UI::DockType::Right);

  Layout();

  EXPECT_EQ(PxRectangle::Create(AreaWidthPx - 30, 0, 30, AreaHeightPx), right->RectPx());
}


TEST_F(TestLayoutDockLayout, Arrange_ChildAtTheBottom_EndsAtTheBottomEdge)
{
  const auto bottom = Child(40, 25);
  m_layout->AddChild(bottom, UI::DockType::Bottom);

  Layout();

  EXPECT_EQ(PxRectangle::Create(0, AreaHeightPx - 25, AreaWidthPx, 25), bottom->RectPx());
}


TEST_F(TestLayoutDockLayout, Arrange_EveryEdgeAndAFill_EachTakesItsBandAndTheFillTheRest)
{
  const auto left = Child(40, 50);
  const auto right = Child(30, 50);
  const auto top = Child(50, 20);
  const auto bottom = Child(50, 25);
  const auto fill = Child(10, 10);
  m_layout->SetLastChildFill(true);
  m_layout->AddChild(left, UI::DockType::Left);
  m_layout->AddChild(right, UI::DockType::Right);
  m_layout->AddChild(top, UI::DockType::Top);
  m_layout->AddChild(bottom, UI::DockType::Bottom);
  // The edge of the child that fills is not used
  m_layout->AddChild(fill, UI::DockType::Left);

  Layout();

  EXPECT_EQ(PxRectangle::Create(0, 0, 40, AreaHeightPx), left->RectPx());
  EXPECT_EQ(PxRectangle::Create(AreaWidthPx - 30, 0, 30, AreaHeightPx), right->RectPx());
  // Between the two, at the top and at the bottom
  EXPECT_EQ(PxRectangle::Create(40, 0, AreaWidthPx - 70, 20), top->RectPx());
  EXPECT_EQ(PxRectangle::Create(40, AreaHeightPx - 25, AreaWidthPx - 70, 25), bottom->RectPx());
  // All that is left
  EXPECT_EQ(PxRectangle::Create(40, 20, AreaWidthPx - 70, AreaHeightPx - 45), fill->RectPx());
}


TEST_F(TestLayoutDockLayout, Arrange_NoLastChildFill_TheLastChildTakesItsBandOnly)
{
  const auto left = Child(40, 50);
  const auto last = Child(30, 25);
  m_layout->AddChild(left, UI::DockType::Left);
  m_layout->AddChild(last, UI::DockType::Top);

  Layout();

  EXPECT_EQ(PxRectangle::Create(0, 0, 40, AreaHeightPx), left->RectPx());
  EXPECT_EQ(PxRectangle::Create(40, 0, AreaWidthPx - 40, 25), last->RectPx());
}


TEST_F(TestLayoutDockLayout, Arrange_LastChildFillWithOneChild_ItGetsAllOfTheLayout)
{
  const auto fill = Child(10, 10);
  m_layout->SetLastChildFill(true);
  m_layout->AddChild(fill, UI::DockType::Right);

  Layout();

  EXPECT_EQ(PxRectangle::Create(0, 0, AreaWidthPx, AreaHeightPx), fill->RectPx());
}


TEST_F(TestLayoutDockLayout, AChildThatAsksForMoreThanThereIs_DoesNotPushTheOthersOut)
{
  // A bar at the right, a bar at the bottom and content that is far higher than the layout
  const auto right = Child(30, 50);
  const auto bottom = Child(50, 25);
  const auto fill = Child(100, 2000);
  m_layout->SetLastChildFill(true);
  m_layout->AddChild(right, UI::DockType::Right);
  m_layout->AddChild(bottom, UI::DockType::Bottom);
  m_layout->AddChild(fill, UI::DockType::Left);

  Layout();

  EXPECT_EQ(PxRectangle::Create(AreaWidthPx - 30, 0, 30, AreaHeightPx), right->RectPx());
  EXPECT_EQ(PxRectangle::Create(0, AreaHeightPx - 25, AreaWidthPx - 30, 25), bottom->RectPx());
  EXPECT_EQ(PxRectangle::Create(0, 0, AreaWidthPx - 30, AreaHeightPx - 25), fill->RectPx());
  // The layout asks for what its children ask for: the content is above the bar at the bottom
  EXPECT_EQ(PxSize2D::Create(130, 2025), m_layout->DesiredSizePx());
}


TEST_F(TestLayoutDockLayout, LimitToAvailableSpace_TheLayoutAsksForNoMoreThanItIsOffered)
{
  m_layout->SetLastChildFill(true);
  m_layout->SetLimitToAvailableSpace(true);
  m_layout->AddChild(Child(30, 50), UI::DockType::Right);
  m_layout->AddChild(Child(50, 25), UI::DockType::Bottom);
  m_layout->AddChild(Child(100, 2000), UI::DockType::Left);

  Layout();

  EXPECT_EQ(PxSize2D::Create(130, AreaHeightPx), m_layout->DesiredSizePx());
}


TEST_F(TestLayoutDockLayout, Arrange_BandsThatAreWiderThanTheLayout_NoChildGetsLessThanNothing)
{
  const auto left = Child(500, 50);
  const auto right = Child(500, 50);
  const auto fill = Child(10, 10);
  m_layout->SetLastChildFill(true);
  m_layout->AddChild(left, UI::DockType::Left);
  m_layout->AddChild(right, UI::DockType::Right);
  m_layout->AddChild(fill, UI::DockType::Left);

  Layout();

  EXPECT_EQ(PxRectangle::Create(0, 0, 500, AreaHeightPx), left->RectPx());
  // It ends at the right edge, over the child at the left
  EXPECT_EQ(PxRectangle::Create(AreaWidthPx - 500, 0, 500, AreaHeightPx), right->RectPx());
  // Nothing is left
  EXPECT_EQ(PxRectangle::Create(500, 0, 0, AreaHeightPx), fill->RectPx());
}
