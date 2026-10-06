#ifndef FSLSIMPLEUI_BASE_EVENT_WINDOWINPUTSCROLLWHEELEVENT_HPP
#define FSLSIMPLEUI_BASE_EVENT_WINDOWINPUTSCROLLWHEELEVENT_HPP
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

#include <FslBase/BasicTypes.hpp>
#include <FslBase/Math/Pixel/PxPoint2.hpp>
#include <FslBase/Math/Pixel/PxValueF.hpp>
#include <FslBase/Time/MillisecondTickCount32.hpp>
#include <FslSimpleUI/Base/Event/WindowEvent.hpp>

namespace Fsl::UI
{
  //! @brief The scroll wheel was turned (or a touchpad was scrolled) with the pointer over a window.
  //! @note  It goes to the window under the pointer that has WindowFlags::ScrollWheelInput set: first down to it from the root
  //!        (OnScrollWheelInputPreview), then back up from it (OnScrollWheelInput), through the windows on the way that have the flag
  //!        too. It is a event of its own, with no begin and no end. A window that acts on it marks it as handled, and a window that
  //!        gets a handled event leaves it alone.
  class WindowInputScrollWheelEvent : public WindowEvent
  {
    MillisecondTickCount32 m_timestamp;
    int32_t m_sourceId{0};
    PxPoint2 m_screenPositionPx;
    PxValueF m_scrollDeltaPxf;

  public:
    WindowInputScrollWheelEvent() noexcept;

    [[nodiscard]] MillisecondTickCount32 GetTimestamp() const noexcept
    {
      return m_timestamp;
    }

    //! @brief The device the event came from
    [[nodiscard]] int32_t GetSourceId() const noexcept
    {
      return m_sourceId;
    }

    //! @brief Return the screen position of the pointer in pixels.
    //! @warning This is not the window position so convert it to window coordinates before using it!!!!)
    [[nodiscard]] PxPoint2 GetScreenPosition() const noexcept
    {
      return m_screenPositionPx;
    }

    //! @brief How far to scroll, in pixels. Positive when the wheel was turned away from the user, which moves the content down
    //!        (towards its start). A notch of a wheel is a fixed distance in dp, a touchpad gives parts of it.
    [[nodiscard]] PxValueF GetScrollDeltaPxf() const noexcept
    {
      return m_scrollDeltaPxf;
    }

  protected:
    // NOLINTNEXTLINE(readability-identifier-naming)
    void SYS_Construct(const MillisecondTickCount32 timestamp, const int32_t sourceId, const PxPoint2& screenPositionPx,
                       const PxValueF scrollDeltaPxf) noexcept;
    // NOLINTNEXTLINE(readability-identifier-naming)
    void SYS_Destruct() noexcept override;
    friend class WindowEventPool;
  };
}

#endif
