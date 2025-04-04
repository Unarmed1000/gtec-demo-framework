#ifndef FSLSIMPLEUI_BASE_GESTURE_EVENT_GESTURETAPBASICEVENT_HPP
#define FSLSIMPLEUI_BASE_GESTURE_EVENT_GESTURETAPBASICEVENT_HPP
/****************************************************************************************************************************************************
 * Copyright 2024 NXP
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
 *    * Neither the name of the NXP. nor the names of
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
#include <FslBase/Math/Pixel/PxPoint2.hpp>
#include <FslBase/Math/Pixel/PxVector2.hpp>
#include <FslSimpleUI/Base/Gesture/Event/GestureBasicEvent.hpp>
#include <cassert>

namespace Fsl::UI
{
  struct GestureTapBasicEvent
  {
    PxPoint2 PositionPx;


    explicit constexpr GestureTapBasicEvent(const PxPoint2 positionPx) noexcept
    {
      PositionPx = positionPx;
    }


    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator GestureBasicEvent() const noexcept
    {
      return {GestureEventType::GestureTap, PositionPx.X.Value, PositionPx.Y.Value};
    }


    static constexpr GestureTapBasicEvent Decode(const GestureBasicEvent& message)
    {
      if (message.Type != GestureEventType::GestureTap)
      {
        throw std::invalid_argument("The message is not of the right type");
      }
      return UncheckedDecode(message);
    }


    static constexpr GestureTapBasicEvent UncheckedDecode(const GestureBasicEvent& message) noexcept
    {
      assert(message.Type == GestureEventType::GestureTap);

      return GestureTapBasicEvent(PxPoint2::Create(message.Param0, message.Param1));
    }
  };
}
#endif
