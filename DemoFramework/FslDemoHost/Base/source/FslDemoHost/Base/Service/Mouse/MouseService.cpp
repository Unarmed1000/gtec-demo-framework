/****************************************************************************************************************************************************
 * Copyright (c) 2014 Freescale Semiconductor, Inc.
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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoApp/Base/Service/Events/Basic/MouseButtonEvent.hpp>
#include <FslDemoApp/Base/Service/Events/Basic/MouseMoveEvent.hpp>
#include <FslDemoApp/Base/Service/Events/Basic/MouseWheelEvent.hpp>
#include <FslDemoApp/Base/Service/Events/Basic/RawMouseMoveEvent.hpp>
#include <FslDemoApp/Base/Service/NativeWindowEvents/INativeWindowEvents.hpp>
#include <FslDemoHost/Base/Service/Events/IEventPoster.hpp>
#include <FslDemoHost/Base/Service/Mouse/MouseService.hpp>
#include <FslNativeWindow/Base/NativeWindowEventHelper.hpp>

namespace Fsl
{
  MouseService::MouseService(const ServiceProvider& serviceProvider)
    : ThreadLocalService(serviceProvider)
    , m_buttonState(0)
  {
  }


  MouseService::~MouseService() = default;


  void MouseService::Link(const ServiceProvider& serviceProvider)
  {
    ThreadLocalService::Link(serviceProvider);
    const std::shared_ptr<INativeWindowEvents> events = serviceProvider.Get<INativeWindowEvents>();
    events->Register(shared_from_this());

    m_eventPoster = serviceProvider.Get<IEventPoster>();
  }


  MouseState MouseService::GetState()
  {
    const auto rawPosition = m_rawPosition;
    m_rawPosition = {};
    return {m_buttonState, m_position, rawPosition};
  }


  void MouseService::OnNativeWindowEvent(const NativeWindowEvent& event)
  {
    switch (event.Type)
    {
    case NativeWindowEventType::InputMouseButton:
      OnMouseButton(event);
      break;
    case NativeWindowEventType::InputMouseMove:
      OnMouseMove(event);
      break;
    case NativeWindowEventType::InputMouseWheel:
      OnMouseWheel(event);
      break;
    case NativeWindowEventType::InputRawMouseMove:
      OnRawMouseMove(event);
      break;
    default:
      break;
    }
  }


  void MouseService::OnMouseButton(const NativeWindowEvent& event)
  {
    VirtualMouseButton button = VirtualMouseButton::Undefined;
    bool isPressed = false;
    bool isTouch = false;
    NativeWindowEventHelper::DecodeInputMouseButtonEvent(event, button, isPressed, m_position, isTouch);

    if (isPressed)
    {
      // Check if the button was unpressed before
      if (!m_buttonState.IsEnabled(button))
      {
        m_buttonState.SetFlag(button, true);

        m_eventPoster->Post(MouseButtonEvent(event.Timestamp, button, isPressed, m_position, isTouch));
      }
    }
    else
    {
      // Check if the button was pressed before
      if (m_buttonState.IsEnabled(button))
      {
        m_buttonState.SetFlag(button, false);

        m_eventPoster->Post(MouseButtonEvent(event.Timestamp, button, isPressed, m_position, isTouch));
      }
    }
  }


  void MouseService::OnMouseMove(const NativeWindowEvent& event)
  {
    VirtualMouseButtonFlags mouseButtonFlags;
    bool isTouch = false;
    NativeWindowEventHelper::DecodeInputMouseMoveEvent(event, m_position, mouseButtonFlags, isTouch);

    if (mouseButtonFlags.IsUndefined())
    {
      mouseButtonFlags = m_buttonState;
    }

    m_eventPoster->Post(MouseMoveEvent(event.Timestamp, m_position, mouseButtonFlags, isTouch));
    // FSLLOG3_INFO("X: {} Y: {}", m_position.X, m_position.Y);
  }


  void MouseService::OnMouseWheel(const NativeWindowEvent& event)
  {
    int32_t delta = 0;
    NativeWindowEventHelper::DecodeInputMouseWheelEvent(event, delta, m_position);

    m_eventPoster->Post(MouseWheelEvent(event.Timestamp, delta, m_position));
  }


  void MouseService::OnRawMouseMove(const NativeWindowEvent& event)
  {
    PxPoint2 newRawPosition;
    VirtualMouseButtonFlags mouseButtonFlags;
    NativeWindowEventHelper::DecodeInputRawMouseMoveEvent(event, newRawPosition, mouseButtonFlags);

    // if (mouseButtonFlags.IsUndefined())
    //  mouseButtonFlags = m_buttonState;

    m_eventPoster->Post(RawMouseMoveEvent(event.Timestamp, newRawPosition, mouseButtonFlags));
    // FSLLOG3_INFO("RawX: {} RawY: {}", m_rawPosition.X, m_rawPosition.Y);

    m_rawPosition += newRawPosition;
  }
}
