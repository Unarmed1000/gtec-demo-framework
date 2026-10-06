#ifndef FSLSIMPLEUI_BASE_CONTROL_SCROLLGESTUREHANDLER_HPP
#define FSLSIMPLEUI_BASE_CONTROL_SCROLLGESTUREHANDLER_HPP
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

#include <FslBase/Math/Pixel/PxPoint2.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Math/Pixel/PxValueF.hpp>
#include <FslBase/Math/Pixel/PxVector2.hpp>
#include <FslBase/Math/Vector2.hpp>
#include <FslBase/Transition/TransitionPxVector2.hpp>
#include <FslGraphics/Sprite/SpriteUnitConverter.hpp>
#include <FslSimpleUI/Base/Control/ContentControl.hpp>
#include <FslSimpleUI/Base/Control/ScrollGestureAnimationConfig.hpp>
#include <FslSimpleUI/Base/Control/ScrollModeFlags.hpp>
#include <FslSimpleUI/Base/Event/EventTransactionState.hpp>
#include <FslSimpleUI/Base/Gesture/GestureManager.hpp>
#include <FslSimpleUI/Base/MovementOwnership.hpp>
#include <FslSimpleUI/Base/MovementTransactionAction.hpp>
#include <vector>


namespace Fsl::UI
{
  class ScrollGestureHandler final
  {
    enum class DragStatus
    {
      Idle,
      Dragging,
      Completed
    };

    enum class AnimStatus
    {
      Idle,
      Flick,
      Bounce
    };

    struct DragRecord
    {
      DragStatus Status{DragStatus::Idle};
      PxPoint2 OffsetPx;
      DpPoint2F DragEndFlickVelocityDpf;
    };

    struct AnimRecord
    {
      AnimStatus Status{AnimStatus::Idle};
      TransitionPxVector2 Anim;

      AnimRecord()
        : Anim(TimeSpan::FromSeconds(1), TransitionType::EaseOutSine)
      {
      }
    };

    ScrollModeFlags m_scrollMode{ScrollModeFlags::Translate};
    SpriteUnitConverter m_unitConverter;
    Fsl::UI::GestureManager m_gestureManager;
    ScrollGestureAnimationConfig m_config;

    DragRecord m_dragRecord;
    AnimRecord m_animRecord;

    PxPoint2 m_scrollOffsetPx;

    //! The distance to scroll by that waits for the next arrange (the scroll wheel), and the part of a pixel that was left over
    float m_pendingScrollDeltaPxf{0.0f};
    float m_scrollDeltaRemainderPxf{0.0f};

  public:
    explicit ScrollGestureHandler(const uint16_t densityDpi);

    void ConfigurationChanged(const uint16_t densityDpi);

    MovementTransactionAction AddMovement(const MillisecondTickCount32 eventTimestamp, const PxPoint2 screenPositionPx,
                                          const EventTransactionState state, const bool isRepeat, const MovementOwnership movementOwnership);

    [[nodiscard]] bool IsScrollingEnabled() const noexcept;

    void SetScrollMode(const ScrollModeFlags value) noexcept;

    //! @brief update the animation
    //! @return true if the layout has been modified, false otherwise
    bool UpdateAnimation(const TimeSpan timeSpan, const ScrollGestureAnimationConfig& config);
    bool UpdateAnimationState(const bool forceCompleteAnimation);

    //! @brief Scroll by a distance in pixels (the scroll wheel). Positive moves the content down or right, towards its start.
    //! @note  The distance is applied by the next Arrange, which knows the range: on the vertical axis when that scrolls and else on
    //!        the horizontal one, and no further than the ends of the content (no overscroll and no bounce). A flick or bounce that is
    //!        running ends where the content is shown. Calls before the arrange add up, and so do parts of a pixel over time.
    //! @return true if the distance was taken and a arrange is needed, false if scrolling is not enabled or a drag is in progress
    bool AddScrollDelta(const PxValueF deltaPxf) noexcept;

    PxPoint2 Arrange(const PxSize2D finalAreaRenderSizePx, const PxSize2D contentRenderSizePx);

  private:
    void ApplyPendingScrollDelta(const PxSize2D scrollSizePx);
    bool IsScrollingRequired(const PxSize2D finalAreaRenderSizePx, const PxSize2D contentRenderSizePx) noexcept;

    void BeginDrag(const PxPoint2 positionPx);
    bool TryDrag(const PxPoint2 positionPx);
    bool TryDragEnd(const PxPoint2 positionPx, const DpPoint2F flickVelocityDpf);
    bool TryCancelDrag();

    PxPoint2 ArrangeNow(const PxSize2D scrollSizePx);
    void SelectAnimation(const PxPoint2 finalPositionPx, const PxSize2D scrollSizePx);
    void SelectFlickAnimation(const float timeToRest, const DpPoint2F dragFlickAccelerationDpf, const PxPoint2 finalPositionPx,
                              const DpPoint2F dragEndFlickVelocityDpf);
  };
}


#endif
