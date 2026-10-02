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
#include <FslSimpleUI/Base/Control/Logic/ButtonPressLogic.hpp>

using namespace Fsl;

namespace
{
  using TestControlLogic_ButtonPressLogic = TestFixtureFslBase;

  // A toggle button of 200x40 with its graphic (the claim rectangle) at the right
  constexpr PxSize2D RenderSizePx = PxSize2D::Create(200, 40);
  constexpr PxRectangle ClaimRectanglePx(PxValue(150), PxValue(0), PxValue(50), PxValue(40));

  UI::ButtonPressInput CreateInput(const UI::EventTransactionState state, const bool isRepeat, const int32_t xPx, const int32_t yPx)
  {
    UI::ButtonPressInput input;
    input.State = state;
    input.IsRepeat = isRepeat;
    input.PositionPx = PxPoint2::Create(xPx, yPx);
    input.RenderSizePx = RenderSizePx;
    input.ClaimRectanglePx = ClaimRectanglePx;
    return input;
  }

  UI::ButtonPressInput Press(const int32_t xPx, const int32_t yPx = 20)
  {
    return CreateInput(UI::EventTransactionState::Begin, false, xPx, yPx);
  }

  UI::ButtonPressInput Move(const int32_t xPx, const int32_t yPx = 20)
  {
    return CreateInput(UI::EventTransactionState::Begin, true, xPx, yPx);
  }

  UI::ButtonPressInput Release(const int32_t xPx, const int32_t yPx = 20)
  {
    return CreateInput(UI::EventTransactionState::End, false, xPx, yPx);
  }

  UI::ButtonPressInput Cancel(const int32_t xPx, const int32_t yPx = 20)
  {
    return CreateInput(UI::EventTransactionState::Canceled, false, xPx, yPx);
  }
}


TEST(TestControlLogic_ButtonPressLogic, Construct)
{
  const UI::ButtonPressLogic logic;

  EXPECT_FALSE(logic.IsDown());
  EXPECT_FALSE(logic.IsClaimed());
}


TEST(TestControlLogic_ButtonPressLogic, PressOutsideClaim_IsHandled_OnEveryEvent)
{
  UI::ButtonPressLogic logic;

  auto result = logic.Process(Press(40));
  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_EQ(UI::ButtonPressAction::Pressed, result.Action);
  EXPECT_TRUE(logic.IsDown());
  EXPECT_FALSE(logic.IsClaimed());

  // Handled on every repeat as well, so a ScrollViewer can still take the gesture
  result = logic.Process(Move(42, 25));
  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_EQ(UI::ButtonPressAction::NoAction, result.Action);
}


TEST(TestControlLogic_ButtonPressLogic, PressInsideClaim_IsClaimed_OnEveryEvent)
{
  UI::ButtonPressLogic logic;

  auto result = logic.Process(Press(170));
  EXPECT_EQ(UI::EventHandlingStatus::Claimed, result.HandlingStatus);
  EXPECT_EQ(UI::ButtonPressAction::Pressed, result.Action);
  EXPECT_TRUE(logic.IsClaimed());

  // The claim is kept when the pointer leaves the claim rectangle, and even the control
  result = logic.Process(Move(40));
  EXPECT_EQ(UI::EventHandlingStatus::Claimed, result.HandlingStatus);
  result = logic.Process(Move(40, 300));
  EXPECT_EQ(UI::EventHandlingStatus::Claimed, result.HandlingStatus);
  EXPECT_TRUE(logic.IsClaimed());
}


TEST(TestControlLogic_ButtonPressLogic, PressOutsideClaim_MovingIntoIt_DoesNotClaim)
{
  UI::ButtonPressLogic logic;
  logic.Process(Press(40));

  const auto result = logic.Process(Move(170));

  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_FALSE(logic.IsClaimed());
}


TEST(TestControlLogic_ButtonPressLogic, ReleaseOnTheButton_IsAClick)
{
  for (const int32_t pressXPx : {40, 170})
  {
    UI::ButtonPressLogic logic;
    logic.Process(Press(pressXPx));

    const auto result = logic.Process(Release(100));

    EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
    EXPECT_EQ(UI::ButtonPressAction::Released, result.Action);
    EXPECT_FALSE(logic.IsDown());
    EXPECT_FALSE(logic.IsClaimed());
  }
}


TEST(TestControlLogic_ButtonPressLogic, ReleaseOutsideTheButton_IsNoClick_ClaimedOrNot)
{
  for (const int32_t pressXPx : {40, 170})
  {
    UI::ButtonPressLogic logic;
    logic.Process(Press(pressXPx));

    const auto result = logic.Process(Release(100, 60));

    EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
    EXPECT_EQ(UI::ButtonPressAction::ReleasedCanceled, result.Action);
    EXPECT_FALSE(logic.IsDown());
  }
}


TEST(TestControlLogic_ButtonPressLogic, ReleaseOnTheEdge_IsHalfOpen)
{
  UI::ButtonPressLogic logic;
  logic.Process(Press(40));
  EXPECT_EQ(UI::ButtonPressAction::Released, logic.Process(Release(199, 39)).Action);

  logic.Process(Press(40));
  EXPECT_EQ(UI::ButtonPressAction::ReleasedCanceled, logic.Process(Release(200, 39)).Action);

  logic.Process(Press(40));
  EXPECT_EQ(UI::ButtonPressAction::ReleasedCanceled, logic.Process(Release(199, 40)).Action);
}


TEST(TestControlLogic_ButtonPressLogic, Cancel_IsNoClick)
{
  // What a ScrollViewer does when it takes the gesture
  UI::ButtonPressLogic logic;
  logic.Process(Press(40));

  const auto result = logic.Process(Cancel(40));

  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_EQ(UI::ButtonPressAction::ReleasedCanceled, result.Action);
  EXPECT_FALSE(logic.IsDown());
}


TEST(TestControlLogic_ButtonPressLogic, EventsWithoutAPress_AreLeftUnhandled)
{
  UI::ButtonPressLogic logic;

  for (const UI::ButtonPressInput& input : {Move(40), Release(40), Cancel(40)})
  {
    const auto result = logic.Process(input);
    EXPECT_EQ(UI::EventHandlingStatus::Unhandled, result.HandlingStatus);
    EXPECT_EQ(UI::ButtonPressAction::NoAction, result.Action);
  }
  EXPECT_FALSE(logic.IsDown());
}


TEST(TestControlLogic_ButtonPressLogic, EmptyClaimRectangle_NeverClaims)
{
  UI::ButtonPressLogic logic;
  UI::ButtonPressInput input = Press(170);
  input.ClaimRectanglePx = {};

  const auto result = logic.Process(input);

  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_FALSE(logic.IsClaimed());
}


TEST(TestControlLogic_ButtonPressLogic, ReleaseAnyHeldPress)
{
  UI::ButtonPressLogic logic;
  EXPECT_FALSE(logic.ReleaseAnyHeldPress());

  logic.Process(Press(170));
  EXPECT_TRUE(logic.ReleaseAnyHeldPress());
  EXPECT_FALSE(logic.IsDown());

  // The release that follows is not for the button any more: no click
  const auto result = logic.Process(Release(170));
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, result.HandlingStatus);
  EXPECT_EQ(UI::ButtonPressAction::NoAction, result.Action);
}


TEST(TestControlLogic_ButtonPressLogic, NewPress_ReplacesAHeldPress)
{
  UI::ButtonPressLogic logic;
  logic.Process(Press(170));
  ASSERT_TRUE(logic.IsClaimed());

  const auto result = logic.Process(Press(40));

  EXPECT_EQ(UI::EventHandlingStatus::Handled, result.HandlingStatus);
  EXPECT_EQ(UI::ButtonPressAction::Pressed, result.Action);
  EXPECT_TRUE(logic.IsDown());
  EXPECT_FALSE(logic.IsClaimed());
}
