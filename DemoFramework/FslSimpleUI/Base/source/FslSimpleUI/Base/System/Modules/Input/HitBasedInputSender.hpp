#ifndef FSLSIMPLEUI_BASE_SYSTEM_MODULES_INPUT_HITBASEDINPUTSENDER_HPP
#define FSLSIMPLEUI_BASE_SYSTEM_MODULES_INPUT_HITBASEDINPUTSENDER_HPP
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

#include <FslBase/BasicTypes.hpp>
#include <FslBase/Math/Pixel/PxValueF.hpp>
#include <FslBase/Time/MillisecondTickCount32.hpp>
#include <FslSimpleUI/Base/Event/EventTransactionState.hpp>
#include <memory>

namespace Fsl
{
  struct PxPoint2;

  namespace UI
  {
    class IModuleHost;
    class IStateEventSender;
    class SimpleEventSender;
    class TreeNode;
    class WindowEventPool;

    class HitBasedInputSender
    {
      std::shared_ptr<IStateEventSender> m_stateEventSenderClickEvent;
      std::shared_ptr<IStateEventSender> m_stateEventSenderMouseOverEvent;
      //! The scroll wheel event has no state, so it is sent with the sender for events of their own
      std::shared_ptr<SimpleEventSender> m_simpleEventSender;
      std::shared_ptr<WindowEventPool> m_windowEventPool;

    public:
      explicit HitBasedInputSender(const std::shared_ptr<IModuleHost>& moduleHost);
      ~HitBasedInputSender();

      [[nodiscard]] bool HasActiveClickEvent() const noexcept;
      [[nodiscard]] bool HasActiveClickEventThatIsNot(const std::shared_ptr<TreeNode>& target) const;
      //! true from the begin of a click to its end or its cancel, also for a click no window took
      [[nodiscard]] bool HasClickHistory() const;
      //! true from the begin of a mouse over to its end or its cancel
      [[nodiscard]] bool HasMouseOverHistory() const;

      bool SendMouseOverEvent(const MillisecondTickCount32 timestamp, const int32_t sourceId, const int32_t sourceSubId,
                              const EventTransactionState state, const bool isRepeat, const PxPoint2& screenPositionPx,
                              const std::shared_ptr<TreeNode>& target);

      bool SendInputClickEvent(const MillisecondTickCount32 timestamp, const int32_t sourceId, const int32_t sourceSubId,
                               const EventTransactionState state, const bool isRepeat, const PxPoint2& screenPositionPx);

      //! @brief Send a scroll wheel event to the given window (and, as a paired event, through its parents that take the wheel)
      //! @return true if a window handled it
      bool SendScrollWheelEvent(const MillisecondTickCount32 timestamp, const int32_t sourceId, const PxPoint2& screenPositionPx,
                                const PxValueF scrollDeltaPxf, const std::shared_ptr<TreeNode>& target);
    };
  }
}

#endif
