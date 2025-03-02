#ifndef FSLSIMPLEUI_BASE_GESTURE_GESTUREDETECTOR_HPP
#define FSLSIMPLEUI_BASE_GESTURE_GESTUREDETECTOR_HPP
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

#include <FslBase/Math/Dp/DpPoint2F.hpp>
#include <FslBase/Time/MillisecondTickCount32.hpp>
#include <FslSimpleUI/Base/Event/EventTransactionState.hpp>
#include <FslSimpleUI/Base/Gesture/GestureAxis.hpp>
#include <FslSimpleUI/Base/Gesture/GestureRecord.hpp>
#include <FslSimpleUI/Base/Gesture/GestureTransactionRecord.hpp>
#include <FslSimpleUI/Base/Gesture/Velocity/VelocityTracker.hpp>
#include <FslSimpleUI/Base/MovementOwnership.hpp>
#include <FslSimpleUI/Base/MovementTransactionAction.hpp>
#include <queue>

namespace Fsl::UI
{
  class GestureDetector final
  {
    GestureFlags m_enabledGestures{GestureFlags::NotDefined};
    VelocityTracker m_velocityTracker;
    GestureAxis m_axisFlags{GestureAxis::NotDefined};
    GestureTransactionRecord m_transactionRecord;
    std::queue<GestureRecord> m_gestureQueue;

  public:
    GestureDetector(const GestureDetector&) = delete;
    GestureDetector& operator=(const GestureDetector&) = delete;

    // move assignment operator
    GestureDetector& operator=(GestureDetector&& other) noexcept;
    // move constructor
    GestureDetector(GestureDetector&& other) noexcept;

    explicit GestureDetector(const GestureFlags enabledGestures, const GestureAxis axisFlags = GestureAxis::XY);

    void Clear();

    //! Check if there is a gesture available
    bool IsGestureAvailable() const noexcept;

    bool InMomementTransaction() const noexcept;

    GestureAxis GetGestureAxis() const noexcept;
    void SetGestureAxis(const GestureAxis value);

    bool HasVelocityEntries() const noexcept;

    bool TryReadGesture(GestureRecord& record) noexcept;

    MovementTransactionAction AddMovement(const MillisecondTickCount32 timestamp, const DpPoint2F screenPositionDpf,
                                          const EventTransactionState state, const bool isRepeat, const MovementOwnership movementOwnership);

  private:
    MovementTransactionAction BeginTransaction(const MillisecondTickCount32 timestamp, const DpPoint2F screenPositionDpf,
                                               const MovementOwnership movementOwnership);
    MovementTransactionAction ContinueTransaction(const MillisecondTickCount32 timestamp, const DpPoint2F screenPositionDpf,
                                                  const MovementOwnership movementOwnership);
    MovementTransactionAction EndTransaction(const MillisecondTickCount32 timestamp, const DpPoint2F screenPositionDpf);
    MovementTransactionAction CancelTransaction(const MillisecondTickCount32 timestamp, const DpPoint2F screenPositionDpf,
                                                const MovementOwnership movementOwnership);
    void UpdateTransactionState(const MillisecondTickCount32 timestamp, const DpPoint2F screenPositionDpf, const bool allowNewGestures);
    void EnqueueGesture(const GestureRecord record);

    void Reset() noexcept;
  };
}

#endif
