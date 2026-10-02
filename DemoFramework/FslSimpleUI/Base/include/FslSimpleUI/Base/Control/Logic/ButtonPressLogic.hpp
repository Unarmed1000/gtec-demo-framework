#ifndef FSLSIMPLEUI_BASE_CONTROL_LOGIC_BUTTONPRESSLOGIC_HPP
#define FSLSIMPLEUI_BASE_CONTROL_LOGIC_BUTTONPRESSLOGIC_HPP
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

#include <FslBase/Math/Pixel/PxPoint2.hpp>
#include <FslBase/Math/Pixel/PxRectangle.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslSimpleUI/Base/Event/EventHandlingStatus.hpp>
#include <FslSimpleUI/Base/Event/EventTransactionState.hpp>

namespace Fsl::UI
{
  //! One click event as a button sees it
  struct ButtonPressInput
  {
    EventTransactionState State{EventTransactionState::Begin};
    //! True for the events that follow the press while it is held (the pointer moved)
    bool IsRepeat{false};
    //! The position of the pointer, relative to the button
    PxPoint2 PositionPx;
    //! The size the button is rendered at: a release inside it is accepted
    PxSize2D RenderSizePx;
    //! The area of the button that claims a gesture that begins inside it (empty: the button never claims)
    PxRectangle ClaimRectanglePx;
  };

  //! What a click event did to the press of a button
  enum class ButtonPressAction
  {
    //! Nothing changed
    NoAction,
    //! The button was pressed
    Pressed,
    //! The press was released on the button: the button was clicked (activate it now)
    Released,
    //! The press ended without a click: released outside the button, or canceled
    ReleasedCanceled
  };

  struct ButtonPressResult
  {
    //! The status to mark the event with
    EventHandlingStatus HandlingStatus{EventHandlingStatus::Unhandled};
    ButtonPressAction Action{ButtonPressAction::NoAction};
  };

  //! The press of a button: when a click happens, and who owns the gesture of the press.
  //!
  //! A button can be inside a ScrollViewer, which wants the same pointer movement. The status a control marks a click event with tells
  //! the ScrollViewer what it may do:
  //! - Unhandled: the ScrollViewer takes the gesture at once.
  //! - Handled: the button uses the press, but the ScrollViewer may still take the gesture once the pointer has moved far enough to be a
  //!   scroll. The button is then sent a canceled click.
  //! - Claimed: the gesture belongs to the button, the ScrollViewer never scrolls on it.
  //!
  //! A press is handled, unless it begins inside the claim rectangle: then it is claimed. That is decided at the press and kept for the
  //! whole gesture, and as the status is read again for every event it is marked on the press and on every repeat. Only a release on the
  //! button is a click; a release outside it and a canceled click end the press without one, claimed or not. Events while no press is
  //! held are left unhandled.
  class ButtonPressLogic final
  {
    enum class PressState
    {
      Up,
      Down,
      DownClaimed
    };

    PressState m_state{PressState::Up};

  public:
    //! @brief True while the button is held down
    [[nodiscard]] bool IsDown() const noexcept
    {
      return m_state != PressState::Up;
    }

    //! @brief True while the held press is claimed (it began inside the claim rectangle)
    [[nodiscard]] bool IsClaimed() const noexcept
    {
      return m_state == PressState::DownClaimed;
    }

    //! @brief Give up a held press without a click (for a button that stops taking clicks while it is held).
    //! @return true if a press was held
    bool ReleaseAnyHeldPress() noexcept
    {
      const bool wasDown = IsDown();
      m_state = PressState::Up;
      return wasDown;
    }

    ButtonPressResult Process(const ButtonPressInput& input) noexcept
    {
      ButtonPressResult result;
      if (input.State == EventTransactionState::Begin)
      {
        if (!input.IsRepeat)
        {
          // A new press replaces a press that is still held (its release was lost)
          const bool isClaimed = !input.ClaimRectanglePx.IsEmpty() && input.ClaimRectanglePx.Contains(input.PositionPx);
          m_state = isClaimed ? PressState::DownClaimed : PressState::Down;
          result.Action = ButtonPressAction::Pressed;
        }
        else if (m_state == PressState::Up)
        {
          // The pointer moves, but no press is held
          return result;
        }
        result.HandlingStatus = m_state == PressState::DownClaimed ? EventHandlingStatus::Claimed : EventHandlingStatus::Handled;
        return result;
      }

      if (m_state == PressState::Up)
      {
        // A release or a cancel while no press is held is not for the button
        return result;
      }
      const bool isAccepted = input.State == EventTransactionState::End && PxRectangle(PxPoint2(), input.RenderSizePx).Contains(input.PositionPx);
      m_state = PressState::Up;
      result.HandlingStatus = EventHandlingStatus::Handled;
      result.Action = isAccepted ? ButtonPressAction::Released : ButtonPressAction::ReleasedCanceled;
      return result;
    }
  };
}

#endif
