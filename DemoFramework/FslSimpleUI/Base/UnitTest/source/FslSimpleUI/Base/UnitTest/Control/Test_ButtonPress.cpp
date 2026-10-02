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

#include <FslBase/Time/MillisecondTickCount32.hpp>
#include <FslSimpleUI/Base/Control/Button.hpp>
#include <FslSimpleUI/Base/Control/ButtonBase.hpp>
#include <FslSimpleUI/Base/Event/EventHandlingStatus.hpp>
#include <FslSimpleUI/Base/Event/EventTransactionState.hpp>
#include <FslSimpleUI/Base/Event/WindowEventPool.hpp>
#include <FslSimpleUI/Base/Event/WindowInputClickEvent.hpp>
#include <FslSimpleUI/Base/PxAvailableSize.hpp>
#include <FslSimpleUI/Base/System/Event/WindowEventQueueEx.hpp>
#include <FslSimpleUI/Base/System/UITree.hpp>
#include <FslSimpleUI/Base/UnitTest/TestFixtureFslSimpleUIUITree.hpp>
#include <memory>
#include <vector>

using namespace Fsl;

namespace
{
  constexpr PxSize2D ButtonSizePx = PxSize2D::Create(200, 40);

  //! What a button was told about its press
  enum class PressCall
  {
    Down,
    Up,
    UpCanceled
  };

  //! A ButtonBase of 200x40 that records its press
  class TestButtonBase final : public UI::ButtonBase
  {
  public:
    std::vector<PressCall> Calls;

    explicit TestButtonBase(const std::shared_ptr<UI::BaseWindowContext>& context)
      : UI::ButtonBase(context)
    {
    }

    void SendClickInput(const std::shared_ptr<UI::WindowInputClickEvent>& theEvent)
    {
      OnClickInput(theEvent);
    }

  protected:
    PxSize2D MeasureOverride(const UI::PxAvailableSize& /*availableSizePx*/) override
    {
      return ButtonSizePx;
    }

    void Pressed(const ButtonPressState state) override
    {
      switch (state)
      {
      case ButtonPressState::Down:
        Calls.push_back(PressCall::Down);
        break;
      case ButtonPressState::Up:
        Calls.push_back(PressCall::Up);
        break;
      case ButtonPressState::UpCancelled:
        Calls.push_back(PressCall::UpCanceled);
        break;
      }
    }
  };

  //! A Button (a content control) of 200x40 that records its press
  class TestButton final : public UI::Button
  {
  public:
    std::vector<PressCall> Calls;

    explicit TestButton(const std::shared_ptr<UI::BaseWindowContext>& context)
      : UI::Button(context)
    {
    }

    void SendClickInput(const std::shared_ptr<UI::WindowInputClickEvent>& theEvent)
    {
      OnClickInput(theEvent);
    }

  protected:
    PxSize2D MeasureOverride(const UI::PxAvailableSize& /*availableSizePx*/) override
    {
      return ButtonSizePx;
    }

    void Down() override
    {
      Calls.push_back(PressCall::Down);
    }

    void Up(const bool wasCanceled) override
    {
      Calls.push_back(wasCanceled ? PressCall::UpCanceled : PressCall::Up);
    }
  };

  template <typename TButton>
  class TestControlButtonPress : public TestFixtureFslSimpleUIUITree
  {
  protected:
    MillisecondTickCount32 m_tickCount;
    std::shared_ptr<TButton> m_button;

  public:
    TestControlButtonPress()
      : m_button(std::make_shared<TButton>(m_windowContext))
    {
      m_tree->AddChild(m_rootWindow, m_button);
      // Lay the button out at the top left corner, so a screen position is the position on the button
      m_tree->Update(TimeSpan(0));
    }

    std::shared_ptr<UI::WindowInputClickEvent> Send(const UI::EventTransactionState state, const bool isRepeat, const int32_t xPx, const int32_t yPx)
    {
      auto theEvent = m_eventPool->AcquireWindowInputClickEvent(m_tickCount, 0, 0, state, isRepeat, PxPoint2::Create(xPx, yPx));
      theEvent->SYS_SetSource(m_button);
      m_button->SendClickInput(theEvent);
      return theEvent;
    }

    std::shared_ptr<UI::WindowInputClickEvent> Press(const int32_t xPx, const int32_t yPx = 20)
    {
      return Send(UI::EventTransactionState::Begin, false, xPx, yPx);
    }

    std::shared_ptr<UI::WindowInputClickEvent> Move(const int32_t xPx, const int32_t yPx = 20)
    {
      return Send(UI::EventTransactionState::Begin, true, xPx, yPx);
    }

    std::shared_ptr<UI::WindowInputClickEvent> Release(const int32_t xPx, const int32_t yPx = 20)
    {
      return Send(UI::EventTransactionState::End, false, xPx, yPx);
    }

    std::shared_ptr<UI::WindowInputClickEvent> Cancel(const int32_t xPx, const int32_t yPx = 20)
    {
      return Send(UI::EventTransactionState::Canceled, false, xPx, yPx);
    }

    //! A click sends a select event
    [[nodiscard]] bool WasClicked() const
    {
      return !m_eventQueue->IsEmpty();
    }
  };

  using ButtonTypes = ::testing::Types<TestButtonBase, TestButton>;
  TYPED_TEST_SUITE(TestControlButtonPress, ButtonTypes);
}


TYPED_TEST(TestControlButtonPress, Construct)
{
  EXPECT_EQ(ButtonSizePx, this->m_button->RenderSizePx());
  EXPECT_TRUE(this->m_button->Calls.empty());
  EXPECT_FALSE(this->WasClicked());
}


TYPED_TEST(TestControlButtonPress, PressThenRelease_OnTheButton_IsAClick)
{
  EXPECT_EQ(UI::EventHandlingStatus::Handled, this->Press(40)->GetHandlingStatus());
  EXPECT_EQ(UI::EventHandlingStatus::Handled, this->Release(100)->GetHandlingStatus());

  EXPECT_EQ((std::vector<PressCall>{PressCall::Down, PressCall::Up}), this->m_button->Calls);
  EXPECT_TRUE(this->WasClicked());
}


TYPED_TEST(TestControlButtonPress, PressAndEveryRepeat_AreOnlyHandled)
{
  // Handled, not claimed: a ScrollViewer the button is in can still take the gesture
  EXPECT_EQ(UI::EventHandlingStatus::Handled, this->Press(40)->GetHandlingStatus());
  EXPECT_EQ(UI::EventHandlingStatus::Handled, this->Move(42, 25)->GetHandlingStatus());
  EXPECT_EQ(UI::EventHandlingStatus::Handled, this->Move(44, 30)->GetHandlingStatus());

  EXPECT_EQ((std::vector<PressCall>{PressCall::Down}), this->m_button->Calls);
}


TYPED_TEST(TestControlButtonPress, Cancel_OnTheButton_IsNoClick)
{
  // What a ScrollViewer does when it takes the gesture: the pointer is still on the button
  this->Press(40);
  this->Move(40, 30);

  EXPECT_EQ(UI::EventHandlingStatus::Handled, this->Cancel(40, 30)->GetHandlingStatus());

  EXPECT_EQ((std::vector<PressCall>{PressCall::Down, PressCall::UpCanceled}), this->m_button->Calls);
  EXPECT_FALSE(this->WasClicked());
}


TYPED_TEST(TestControlButtonPress, ReleaseOutsideTheButton_IsNoClick)
{
  this->Press(40);

  EXPECT_EQ(UI::EventHandlingStatus::Handled, this->Release(100, 40)->GetHandlingStatus());

  EXPECT_EQ((std::vector<PressCall>{PressCall::Down, PressCall::UpCanceled}), this->m_button->Calls);
  EXPECT_FALSE(this->WasClicked());
}


TYPED_TEST(TestControlButtonPress, Disable_WhileHeld_EndsThePressWithoutAClick)
{
  this->Press(40);

  this->m_button->SetEnabled(false);
  EXPECT_EQ((std::vector<PressCall>{PressCall::Down, PressCall::UpCanceled}), this->m_button->Calls);

  // The release is not for the button any more
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, this->Release(100)->GetHandlingStatus());
  EXPECT_EQ((std::vector<PressCall>{PressCall::Down, PressCall::UpCanceled}), this->m_button->Calls);
  EXPECT_FALSE(this->WasClicked());
}


TYPED_TEST(TestControlButtonPress, Hide_WhileHeld_EndsThePressWithoutAClick)
{
  this->Press(40);

  this->m_button->SetVisibility(UI::ItemVisibility::Collapsed);
  EXPECT_EQ((std::vector<PressCall>{PressCall::Down, PressCall::UpCanceled}), this->m_button->Calls);

  // The release is not for the button any more
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, this->Release(100)->GetHandlingStatus());
  EXPECT_EQ((std::vector<PressCall>{PressCall::Down, PressCall::UpCanceled}), this->m_button->Calls);
  EXPECT_FALSE(this->WasClicked());
}


TYPED_TEST(TestControlButtonPress, EventsWithoutAPress_AreLeftUnhandled)
{
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, this->Move(40)->GetHandlingStatus());
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, this->Release(40)->GetHandlingStatus());
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, this->Cancel(40)->GetHandlingStatus());

  EXPECT_TRUE(this->m_button->Calls.empty());
  EXPECT_FALSE(this->WasClicked());
}


TYPED_TEST(TestControlButtonPress, Disabled_IgnoresAPress)
{
  this->m_button->SetEnabled(false);

  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, this->Press(40)->GetHandlingStatus());
  EXPECT_EQ(UI::EventHandlingStatus::Unhandled, this->Release(40)->GetHandlingStatus());

  EXPECT_TRUE(this->m_button->Calls.empty());
  EXPECT_FALSE(this->WasClicked());
}
