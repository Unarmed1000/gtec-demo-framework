#ifndef FSLDEMOAPP_BASE_SERVICE_EVENTS_BASIC_WINDOWFOCUSEVENT_HPP
#define FSLDEMOAPP_BASE_SERVICE_EVENTS_BASIC_WINDOWFOCUSEVENT_HPP
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

#include <FslBase/Exceptions.hpp>
#include <FslDemoApp/Base/Service/Events/Basic/BasicEvent.hpp>

namespace Fsl
{
  //! The window of the app got or lost the input focus of the window system (its keyboard focus). The app keeps running and drawing
  //! without the focus: the event is for what should not go on without it, like a input that was begun and will not be finished.
  //! Only a change is told, and a window system can say nothing about the focus a window has when it starts: a app that was told
  //! nothing takes it that it has the focus.
  // Basic events must be exactly the same size as a BasicEvent (so they can have no member variables).
  class WindowFocusEvent final : public BasicEvent
  {
  public:
    explicit constexpr WindowFocusEvent(const BasicEvent& encodedEvent)
      : BasicEvent(encodedEvent)
    {
      if (m_type != EventType::WindowFocus)
      {
        throw std::invalid_argument("The supplied argument is of a wrong type");
      }
    }


    explicit constexpr WindowFocusEvent(const bool isFocused) noexcept
      : BasicEvent(EventType::WindowFocus, {}, isFocused ? 1 : 0)
    {
    }

    //! true: the window has the input focus from here on, false: it lost it
    constexpr bool IsFocused() const noexcept
    {
      return m_arg1 != 0;
    }
  };
}

#endif
