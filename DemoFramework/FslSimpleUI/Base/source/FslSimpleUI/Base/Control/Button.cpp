/****************************************************************************************************************************************************
 * Copyright (c) 2015 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslGraphics/Color.hpp>
#include <FslSimpleUI/Base/Control/Button.hpp>
#include <FslSimpleUI/Base/Event/WindowEventPool.hpp>
#include <FslSimpleUI/Base/Event/WindowEventSender.hpp>
#include <FslSimpleUI/Base/Event/WindowInputClickEvent.hpp>
#include <FslSimpleUI/Base/Event/WindowSelectEvent.hpp>
#include <FslSimpleUI/Base/PropertyTypeFlags.hpp>
#include <FslSimpleUI/Base/UIDrawContext.hpp>
#include <cassert>

namespace Fsl::UI
{
  Button::Button(const std::shared_ptr<BaseWindowContext>& context)
    : ContentControl(context)
  {
    Enable(WindowFlags::ClickInput);
  }

  void Button::SetEnabled(const bool enable)
  {
    if (enable != m_isEnabled)
    {
      m_isEnabled = enable;
      if (!enable)
      {
        // A disabled button takes no clicks, so a held press ends without a click (the release would be ignored)
        CancelButtonDown();
      }
      PropertyUpdated(PropertyType::Other);
    }
  }


  void Button::OnClickInput(const std::shared_ptr<WindowInputClickEvent>& theEvent)
  {
    if (!theEvent->IsSource(this) || !m_isEnabled || theEvent->IsHandled())
    {
      return;
    }

    if (theEvent->GetState() == EventTransactionState::Begin && !theEvent->IsRepeat() && m_pressLogic.IsDown())
    {
      // A new press while a press is held (its release was lost): the old press ends without a click
      CancelButtonDown();
    }

    // The press is only handled (on the press and on every repeat), so a ScrollViewer the button is in can still turn it into a scroll.
    // The button then gets a canceled click, which is no click. Only a release on the button is a click.
    ButtonPressInput input;
    input.State = theEvent->GetState();
    input.IsRepeat = theEvent->IsRepeat();
    input.PositionPx = PointFromScreen(theEvent->GetScreenPosition());
    input.RenderSizePx = RenderSizePx();
    const ButtonPressResult result = m_pressLogic.Process(input);
    if (result.Status != EventHandlingStatus::Unhandled)
    {
      theEvent->Handled();
    }

    switch (result.Action)
    {
    case ButtonPressAction::Pressed:
      Down();
      break;
    case ButtonPressAction::Released:
      SendEvent(GetEventPool()->AcquireWindowSelectEvent(0));
      Up(false);
      break;
    case ButtonPressAction::ReleasedCanceled:
      Up(true);
      break;
    case ButtonPressAction::NoAction:
    default:
      break;
    }
  }


  void Button::OnPropertiesUpdated(const PropertyTypeFlags& flags)
  {
    base_type::OnPropertiesUpdated(flags);
    // A hidden button takes no clicks, so a held press ends without a click
    if (flags.IsFlagged(PropertyType::Layout) && GetVisibility() != ItemVisibility::Visible)
    {
      CancelButtonDown();
    }
  }


  void Button::CancelButtonDown()
  {
    if (m_pressLogic.ReleaseAnyHeldPress())
    {
      Up(true);
    }
  }
}
