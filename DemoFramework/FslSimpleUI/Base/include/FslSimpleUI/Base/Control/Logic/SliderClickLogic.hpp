#ifndef FSLSIMPLEUI_BASE_CONTROL_LOGIC_SLIDERCLICKLOGIC_HPP
#define FSLSIMPLEUI_BASE_CONTROL_LOGIC_SLIDERCLICKLOGIC_HPP
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
#include <FslBase/Math/Pixel/PxValue.hpp>
#include <FslSimpleUI/Base/Control/Logic/SliderLogic.hpp>
#include <FslSimpleUI/Base/Event/EventHandlingStatus.hpp>
#include <FslSimpleUI/Base/Event/EventTransactionState.hpp>

namespace Fsl::UI
{
  //! One click event as a slider sees it
  struct SliderClickInput
  {
    EventTransactionState State{EventTransactionState::Begin};
    //! True for the events that follow the press while it is held (the pointer moved)
    bool IsRepeat{false};
    //! The position of the pointer, relative to the slider
    PxPoint2 PositionPx;
    //! The area of the slider that takes a press: the bar
    PxRectangle BarRectanglePx;
    //! The area of the cursor of the slider (the handle): a press inside it begins a drag
    PxRectangle CursorGrabRectanglePx;
    //! True if the slider is laid out horizontally (the X position is the one that counts)
    bool IsHorizontal{true};
    //! True if the value of the slider can not be changed by the user
    bool IsReadOnly{false};
  };

  //! What the slider did with a click event
  enum class SliderClickAction
  {
    //! Nothing changed
    NoAction,
    //! A drag began (the value follows the pointer)
    DragBegin,
    //! The value was dragged to a new value
    Drag,
    //! The drag ended, the value is kept
    DragEnd,
    //! The drag was canceled, the value is the one from before the drag
    DragCanceled,
    //! The bar was clicked: the slider should be set to SliderClickResult::Value
    SetValue
  };

  template <typename T>
  struct SliderClickResult
  {
    //! The status to mark the event with
    EventHandlingStatus Status{EventHandlingStatus::Unhandled};
    SliderClickAction Action{SliderClickAction::NoAction};
    //! SliderClickAction::SetValue: the value to set
    T Value{};
  };

  //! Decides what a click event does to a slider, and who owns the gesture the event is part of.
  //!
  //! A slider can be inside a ScrollViewer, which wants the same pointer movement. The status a control marks a click event with tells
  //! the ScrollViewer what it may do:
  //! - Unhandled: the ScrollViewer takes the gesture at once.
  //! - Handled: the control uses the press, but the ScrollViewer may still take the gesture once the pointer has moved far enough to be a
  //!   scroll. The control is then sent a canceled click.
  //! - Claimed: the gesture belongs to the control, the ScrollViewer never scrolls on it.
  //!
  //! So a slider has two kinds of press:
  //! - On its cursor: a drag begins and the gesture is claimed. Without the claim the ScrollViewer would take the drag as soon as the
  //!   pointer moved a little on its scroll axis.
  //! - Elsewhere on its bar: the press is only handled. If it is released on the bar the slider is set to the value of that position,
  //!   and if the pointer moves instead the ScrollViewer can scroll (so a list of sliders can be scrolled by its bars).
  //!
  //! The status is read again for every event of a gesture, so it is marked on every event the slider holds a press for, not just on
  //! the press. A canceled click gives the press up: a drag goes back to the value from before it and a bar click sets nothing.
  template <typename T>
  class SliderClickLogic final
  {
    enum class ClickState
    {
      NotClicked,
      //! The bar was pressed outside the cursor: the value is set when it is released on the bar
      BarClick,
      //! The cursor was pressed: the value follows the pointer
      Dragging
    };

    ClickState m_state{ClickState::NotClicked};

  public:
    using value_type = T;

    //! @brief True while a press on the bar or on the cursor is held
    [[nodiscard]] bool IsClicked() const noexcept
    {
      return m_state != ClickState::NotClicked;
    }

    //! @brief Give up a press that is held. A drag has to be canceled on the slider logic by the caller.
    void Reset() noexcept
    {
      m_state = ClickState::NotClicked;
    }

    //! @brief Process a click event.
    //! @param rLogic the logic of the slider, it is dragged by the event
    SliderClickResult<value_type> Process(SliderLogic<value_type>& rLogic, const SliderClickInput& input)
    {
      SliderClickResult<value_type> result;
      if (!rLogic.IsEnabled())
      {
        // A disabled slider holds nothing (disabling it canceled its drag) and leaves every event to its parents
        m_state = ClickState::NotClicked;
        return result;
      }

      const PxValue offsetPx = input.IsHorizontal ? input.PositionPx.X : input.PositionPx.Y;
      const bool isPress = input.State == EventTransactionState::Begin && !input.IsRepeat;
      if (isPress)
      {
        // A new press replaces whatever was held
        if (rLogic.CancelDrag())
        {
          result.Action = SliderClickAction::DragCanceled;
        }
        m_state = ClickState::NotClicked;
        if (input.IsReadOnly)
        {
          return result;
        }
        if (input.CursorGrabRectanglePx.Contains(input.PositionPx) && rLogic.TryBeginDrag(offsetPx))
        {
          m_state = ClickState::Dragging;
          result.Status = EventHandlingStatus::Claimed;
          result.Action = SliderClickAction::DragBegin;
        }
        else if (input.BarRectanglePx.Contains(input.PositionPx))
        {
          m_state = ClickState::BarClick;
          result.Status = EventHandlingStatus::Handled;
        }
        return result;
      }

      switch (m_state)
      {
      case ClickState::Dragging:
        // The drag is claimed on every event of it
        result.Status = EventHandlingStatus::Claimed;
        if (input.State == EventTransactionState::Begin)
        {
          const value_type oldValue = rLogic.GetValue();
          if (rLogic.TryDrag(offsetPx) && rLogic.GetValue() != oldValue)
          {
            result.Action = SliderClickAction::Drag;
          }
        }
        else
        {
          m_state = ClickState::NotClicked;
          if (input.State == EventTransactionState::Canceled)
          {
            if (rLogic.CancelDrag())
            {
              result.Action = SliderClickAction::DragCanceled;
            }
          }
          else if (rLogic.EndDrag(offsetPx))
          {
            result.Action = SliderClickAction::DragEnd;
          }
        }
        break;
      case ClickState::BarClick:
        // The bar click is only handled, so a ScrollViewer can still take the gesture
        result.Status = EventHandlingStatus::Handled;
        if (input.State != EventTransactionState::Begin)
        {
          m_state = ClickState::NotClicked;
          if (input.State == EventTransactionState::End && input.BarRectanglePx.Contains(input.PositionPx))
          {
            result.Action = SliderClickAction::SetValue;
            result.Value = rLogic.GetValueForPositionPx(offsetPx);
          }
        }
        break;
      case ClickState::NotClicked:
      default:
        // A event without a press that is held is not for the slider
        break;
      }
      return result;
    }
  };
}

#endif
