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

#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslSimpleUI/Base/Control/Logic/SliderClickLogic.hpp>

using namespace Fsl;

namespace
{
  using TestControlLogic_SliderClickLogic = TestFixtureFslBase;

  // A horizontal slider: the bar is 121x20, the value 0 is at x=10 and the value 100 at x=110 (one pixel per value)
  constexpr PxRectangle BarRectanglePx(PxValue(0), PxValue(0), PxValue(121), PxValue(20));
  constexpr PxValue SpanStartPx(10);
  constexpr int32_t CursorHalfWidthPx = 8;

  UI::SliderLogic<int32_t> CreateLogic(const int32_t value)
  {
    return UI::SliderLogic<int32_t>(UI::SliderConstrainedValue<int32_t>(value, 0, 100), UI::SliderPixelSpanInfo(SpanStartPx, PxSize1D::Create(101)));
  }

  //! A event at the given position, the cursor is where the value of the logic puts it
  UI::SliderClickInput CreateInput(const UI::SliderLogic<int32_t>& logic, const UI::EventTransactionState state, const bool isRepeat,
                                   const int32_t xPx, const int32_t yPx = 10)
  {
    UI::SliderClickInput input;
    input.State = state;
    input.IsRepeat = isRepeat;
    input.PositionPx = PxPoint2::Create(xPx, yPx);
    input.BarRectanglePx = BarRectanglePx;
    input.CursorGrabRectanglePx =
      PxRectangle(logic.GetPositionPx() - PxValue(CursorHalfWidthPx), PxValue(0), PxValue(2 * CursorHalfWidthPx), PxValue(20));
    input.IsHorizontal = true;
    return input;
  }

  UI::SliderClickInput Press(const UI::SliderLogic<int32_t>& logic, const int32_t xPx, const int32_t yPx = 10)
  {
    return CreateInput(logic, UI::EventTransactionState::Begin, false, xPx, yPx);
  }

  UI::SliderClickInput Move(const UI::SliderLogic<int32_t>& logic, const int32_t xPx, const int32_t yPx = 10)
  {
    return CreateInput(logic, UI::EventTransactionState::Begin, true, xPx, yPx);
  }

  UI::SliderClickInput Release(const UI::SliderLogic<int32_t>& logic, const int32_t xPx, const int32_t yPx = 10)
  {
    return CreateInput(logic, UI::EventTransactionState::End, false, xPx, yPx);
  }

  UI::SliderClickInput Cancel(const UI::SliderLogic<int32_t>& logic, const int32_t xPx, const int32_t yPx = 10)
  {
    return CreateInput(logic, UI::EventTransactionState::Canceled, false, xPx, yPx);
  }
}


TEST(TestControlLogic_SliderClickLogic, Construct)
{
  const UI::SliderClickLogic<int32_t> clickLogic;

  EXPECT_FALSE(clickLogic.IsClicked());
}


TEST(TestControlLogic_SliderClickLogic, PressOnCursor_BeginsADragThatIsClaimed)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;

  // The cursor is at x=60, the press is two pixels to the right of its center
  const auto result = clickLogic.Process(logic, Press(logic, 62));

  EXPECT_EQ(UI::EventHandlingStatus::Claimed, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::DragBegin, result.Action);
  EXPECT_TRUE(clickLogic.IsClicked());
  EXPECT_TRUE(logic.IsDragging());
  // The cursor is now under the pointer
  EXPECT_EQ(52, logic.GetValue());
}


TEST(TestControlLogic_SliderClickLogic, Drag_IsClaimedOnEveryEvent)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;
  clickLogic.Process(logic, Press(logic, 60));

  // The pointer moves, also far below the bar: a ScrollViewer must not get the gesture
  auto result = clickLogic.Process(logic, Move(logic, 80, 200));
  EXPECT_EQ(UI::EventHandlingStatus::Claimed, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::Drag, result.Action);
  EXPECT_EQ(70, logic.GetValue());

  // A move that does not change the value is still claimed
  result = clickLogic.Process(logic, Move(logic, 80, 300));
  EXPECT_EQ(UI::EventHandlingStatus::Claimed, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::NoAction, result.Action);

  result = clickLogic.Process(logic, Release(logic, 90, 300));
  EXPECT_EQ(UI::EventHandlingStatus::Claimed, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::DragEnd, result.Action);
  EXPECT_EQ(80, logic.GetValue());
  EXPECT_FALSE(logic.IsDragging());
  EXPECT_FALSE(clickLogic.IsClicked());
}


TEST(TestControlLogic_SliderClickLogic, Drag_Canceled_RestoresTheValue)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;
  clickLogic.Process(logic, Press(logic, 60));
  clickLogic.Process(logic, Move(logic, 100));
  ASSERT_EQ(90, logic.GetValue());

  const auto result = clickLogic.Process(logic, Cancel(logic, 100));

  EXPECT_EQ(UI::EventHandlingStatus::Claimed, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::DragCanceled, result.Action);
  EXPECT_EQ(50, logic.GetValue());
  EXPECT_FALSE(logic.IsDragging());
  EXPECT_FALSE(clickLogic.IsClicked());
}


TEST(TestControlLogic_SliderClickLogic, PressOnBar_IsOnlyHandled_AndChangesNothing)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;

  // Far from the cursor at x=60
  const auto result = clickLogic.Process(logic, Press(logic, 100));

  // Handled and not claimed: a ScrollViewer can still turn the gesture into a scroll
  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::NoAction, result.Action);
  EXPECT_TRUE(clickLogic.IsClicked());
  EXPECT_FALSE(logic.IsDragging());
  EXPECT_EQ(50, logic.GetValue());
}


TEST(TestControlLogic_SliderClickLogic, BarClick_Move_StaysHandled_AndChangesNothing)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;
  clickLogic.Process(logic, Press(logic, 100));

  const auto result = clickLogic.Process(logic, Move(logic, 90, 14));

  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::NoAction, result.Action);
  EXPECT_FALSE(logic.IsDragging());
  EXPECT_EQ(50, logic.GetValue());
}


TEST(TestControlLogic_SliderClickLogic, BarClick_ReleasedOnBar_SetsTheValueOfThePosition)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;
  clickLogic.Process(logic, Press(logic, 100));

  const auto result = clickLogic.Process(logic, Release(logic, 95));

  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::SetValue, result.Action);
  EXPECT_EQ(85, result.Value);
  // The value is set by the control (it owns the property), the logic is untouched
  EXPECT_EQ(50, logic.GetValue());
  EXPECT_FALSE(clickLogic.IsClicked());
}


TEST(TestControlLogic_SliderClickLogic, BarClick_ReleasedOutsideBar_SetsNothing)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;
  clickLogic.Process(logic, Press(logic, 100));

  const auto result = clickLogic.Process(logic, Release(logic, 95, 40));

  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::NoAction, result.Action);
  EXPECT_EQ(50, logic.GetValue());
  EXPECT_FALSE(clickLogic.IsClicked());
}


TEST(TestControlLogic_SliderClickLogic, BarClick_Canceled_SetsNothing)
{
  // What a ScrollViewer does when it takes the gesture: the slider gets a canceled click
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;
  clickLogic.Process(logic, Press(logic, 100));

  const auto result = clickLogic.Process(logic, Cancel(logic, 100));

  EXPECT_EQ(UI::SliderClickAction::NoAction, result.Action);
  EXPECT_EQ(50, logic.GetValue());
  EXPECT_FALSE(clickLogic.IsClicked());

  // The release that follows is not for the slider
  const auto releaseResult = clickLogic.Process(logic, Release(logic, 100));
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, releaseResult.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::NoAction, releaseResult.Action);
}


TEST(TestControlLogic_SliderClickLogic, PressOutsideBar_IsLeftUnhandled)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;

  const auto result = clickLogic.Process(logic, Press(logic, 100, 40));

  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::NoAction, result.Action);
  EXPECT_FALSE(clickLogic.IsClicked());
}


TEST(TestControlLogic_SliderClickLogic, ReleaseWithoutPress_IsLeftUnhandled)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;

  auto result = clickLogic.Process(logic, Release(logic, 60));
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::NoAction, result.Action);

  result = clickLogic.Process(logic, Move(logic, 60));
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::NoAction, result.Action);
  EXPECT_EQ(50, logic.GetValue());
}


TEST(TestControlLogic_SliderClickLogic, ReadOnly_TakesNoPress)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;

  auto input = Press(logic, 60);
  input.IsReadOnly = true;
  auto result = clickLogic.Process(logic, input);
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, result.HandlingStatus);
  EXPECT_FALSE(logic.IsDragging());

  input = Press(logic, 100);
  input.IsReadOnly = true;
  result = clickLogic.Process(logic, input);
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, result.HandlingStatus);
  EXPECT_FALSE(clickLogic.IsClicked());
  EXPECT_EQ(50, logic.GetValue());
}


TEST(TestControlLogic_SliderClickLogic, Disabled_TakesNoPress_AndGivesUpAHeldOne)
{
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;
  clickLogic.Process(logic, Press(logic, 100));
  ASSERT_TRUE(clickLogic.IsClicked());

  logic.SetEnabled(false);

  // The release of the bar click sets nothing and is left to the parents
  auto result = clickLogic.Process(logic, Release(logic, 100));
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::NoAction, result.Action);
  EXPECT_FALSE(clickLogic.IsClicked());

  result = clickLogic.Process(logic, Press(logic, 60));
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, result.HandlingStatus);
  EXPECT_FALSE(logic.IsDragging());
  EXPECT_EQ(50, logic.GetValue());
}


TEST(TestControlLogic_SliderClickLogic, Vertical_UsesTheYPosition)
{
  // A vertical slider: the value 0 is at y=10 and the value 100 at y=110
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;

  UI::SliderClickInput input;
  input.State = UI::EventTransactionState::Begin;
  input.PositionPx = PxPoint2::Create(10, 60);
  input.BarRectanglePx = PxRectangle(PxValue(0), PxValue(0), PxValue(20), PxValue(121));
  input.CursorGrabRectanglePx = PxRectangle(PxValue(0), PxValue(52), PxValue(20), PxValue(16));
  input.IsHorizontal = false;
  auto result = clickLogic.Process(logic, input);
  ASSERT_EQ(UI::SliderClickAction::DragBegin, result.Action);

  input.IsRepeat = true;
  input.PositionPx = PxPoint2::Create(300, 30);
  result = clickLogic.Process(logic, input);

  EXPECT_EQ(UI::EventHandlingStatus::Claimed, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::Drag, result.Action);
  EXPECT_EQ(20, logic.GetValue());
}


TEST(TestControlLogic_SliderClickLogic, NewPress_ReplacesAHeldDrag)
{
  // A press that arrives while a drag is held (the release was lost) starts from the value before that drag
  UI::SliderLogic<int32_t> logic = CreateLogic(50);
  UI::SliderClickLogic<int32_t> clickLogic;
  clickLogic.Process(logic, Press(logic, 60));
  clickLogic.Process(logic, Move(logic, 100));
  ASSERT_EQ(90, logic.GetValue());

  // The old drag is canceled, and the press is on the bar away from the cursor (which is back at x=60)
  UI::SliderClickInput input = Press(logic, 20);
  input.CursorGrabRectanglePx = PxRectangle(PxValue(52), PxValue(0), PxValue(16), PxValue(20));
  const auto result = clickLogic.Process(logic, input);

  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_EQ(UI::SliderClickAction::DragCanceled, result.Action);
  EXPECT_FALSE(logic.IsDragging());
  EXPECT_EQ(50, logic.GetValue());
}
