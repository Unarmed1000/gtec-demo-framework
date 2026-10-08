#ifndef FSLSIMPLEUI_BASE_CONTROL_SCROLLVIEWER_HPP
#define FSLSIMPLEUI_BASE_CONTROL_SCROLLVIEWER_HPP
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

#include <FslBase/Time/TimeSpan.hpp>
#include <FslGraphics/Sprite/IContentSprite.hpp>
#include <FslSimpleUI/Base/Control/ContentControl.hpp>
#include <FslSimpleUI/Base/Control/ScrollGestureHandler.hpp>
#include <FslSimpleUI/Base/Control/ScrollModeFlags.hpp>
#include <FslSimpleUI/Base/Mesh/ContentSpriteMesh.hpp>
#include <memory>

namespace Fsl::UI
{
  class ScrollViewer final : public ContentControl
  {
    using base_type = ContentControl;

  protected:
    struct RenderCursorInfo
    {
      bool Enabled{false};
      PxPoint2 OffsetPx;
      PxSize2D SizePx;
      UIRenderColor Color;

      constexpr RenderCursorInfo() noexcept = default;
      constexpr RenderCursorInfo(const bool enabled, const PxPoint2 offsetPx, const PxSize2D sizePx, const UIRenderColor color) noexcept
      {
        Enabled = enabled;
        OffsetPx = offsetPx;
        SizePx = sizePx;
        Color = color;
      }
    };

  private:
    ScrollGestureHandler m_gestureHandler;
    PxPoint2 m_scrollPositionOffsetPx;
    //! The area the content is shown in and the size of what is scrolled, as the last arrange gave them to the gesture handler. The
    //! scrollbar cursors are worked out from the same two sizes the scroll range comes from.
    PxSize2D m_scrollViewSizePx;
    PxSize2D m_scrollExtentPx;
    //! The arrange found that the scroll position has to animate, the animation is started after the layout (WinPostLayout) as the
    //! window flags must not be changed during the layout
    bool m_animationCheckPending{false};
    //! The window MakeVisible was asked for, until the next arrange has scrolled to it
    std::weak_ptr<BaseWindow> m_makeVisibleWindow;
    bool m_makeVisiblePending{false};

    ContentSpriteMesh m_cursorX;
    ContentSpriteMesh m_cursorY;

    DataBinding::TypedDependencyProperty<ScrollModeFlags> m_propertyScrollMode;
    DataBinding::TypedDependencyProperty<float> m_propertyDragFlickDeceleration;
    DataBinding::TypedDependencyProperty<TransitionType> m_propertyDragFlickTransitionType;
    DataBinding::TypedDependencyProperty<float> m_propertyDragEndAnimTimeMultiplier;
    DataBinding::TypedDependencyProperty<float> m_propertyBounceSpringStiffness;
    DataBinding::TypedDependencyProperty<TimeSpan> m_propertyBounceAnimationTime;
    DataBinding::TypedDependencyProperty<TransitionType> m_propertyBounceTransitionType;
    DataBinding::TypedDependencyProperty<bool> m_propertyClipContent;
    DependencyPropertyUIColor m_propertyCursorColor;

  public:
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition PropertyScrollMode;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition PropertyDragFlickDeceleration;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition PropertyDragFlickTransitionType;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition PropertyDragEndAnimTimeMultiplier;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition PropertyBounceSpringStiffness;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition PropertyBounceAnimationTime;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition PropertyBounceTransitionType;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition PropertyClipContent;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition PropertyCursorColor;

    explicit ScrollViewer(const std::shared_ptr<BaseWindowContext>& context);

    void WinResolutionChanged(const ResolutionChangedInfo& info) final;
    void WinPostLayout() final;

    ScrollModeFlags GetScrollMode() const noexcept;
    bool SetScrollMode(const ScrollModeFlags value) noexcept;

    float GetDragFlickDeceleration() const noexcept;
    bool SetDragFlickDeceleration(const float value) noexcept;

    TransitionType GetDragFlickTransitionType() const noexcept;
    bool SetDragFlickTransitionType(const TransitionType value) noexcept;

    float GetDragEndAnimTimeMultiplier() const noexcept;
    bool SetDragEndAnimTimeMultiplier(const float value) noexcept;

    float GetBounceSpringStiffness() const noexcept;
    bool SetBounceSpringStiffness(const float value) noexcept;

    TimeSpan GetBounceAnimationTime() const noexcept;
    bool SetBounceAnimationTime(const TimeSpan value) noexcept;

    TransitionType GetBounceTransitionType() const noexcept;
    bool SetBounceTransitionType(const TransitionType value) noexcept;

    UIColor GetCursorColor() const noexcept;
    bool SetCursorColor(const UIColor value) noexcept;

    bool GetClipContent() const noexcept;
    bool SetClipContent(const bool value) noexcept;

    const std::shared_ptr<IContentSprite>& GetCursorX() const
    {
      return m_cursorX.GetSprite();
    }

    bool SetCursorX(const std::shared_ptr<IContentSprite>& value);
    bool SetCursorX(std::shared_ptr<IContentSprite>&& value);

    const std::shared_ptr<IContentSprite>& GetCursorY() const
    {
      return m_cursorY.GetSprite();
    }

    bool SetCursorY(const std::shared_ptr<IContentSprite>& value);
    bool SetCursorY(std::shared_ptr<IContentSprite>&& value);

    //! @brief Scroll so a window of the content is inside the view (a cursor that is moved with the keys). Nothing moves when it is
    //!        inside already.
    //! @param window a window inside the content of this viewer: the content itself, or a child of it at any depth
    //! @note  The next layout scrolls, once it has placed the content, so the place of the window is the one of that layout: by the
    //!        shortest distance that brings the window in, along the axes that scroll. A window that is larger than the view is
    //!        shown from its start. A drag, flick or bounce ends when the content has to move, and is left alone when it does not.
    //!        The last call before a layout is the one that counts. A window that is not in the content at that layout is ignored.
    void MakeVisible(const std::shared_ptr<BaseWindow>& window);

    void WinDraw(const UIDrawContext& context) override;

  protected:
    void OnClickInput(const std::shared_ptr<WindowInputClickEvent>& theEvent) final;
    void OnScrollWheelInput(const std::shared_ptr<WindowInputScrollWheelEvent>& theEvent) final;

    void UpdateAnimation(const TimeSpan& timeSpan) final;
    bool UpdateAnimationState(const bool forceCompleteAnimation) final;
    PxSize2D MeasureOverride(const PxAvailableSize& availableSizePx) final;
    PxSize2D ArrangeOverride(const PxSize2D& finalSizePx) final;

    RenderCursorInfo GetRenderCursorInfoX() const noexcept;
    RenderCursorInfo GetRenderCursorInfoY() const noexcept;

    DataBinding::DataBindingInstanceHandle TryGetPropertyHandleNow(const DataBinding::DependencyPropertyDefinition& sourceDef) final;
    DataBinding::PropertySetBindingResult TrySetBindingNow(const DataBinding::DependencyPropertyDefinition& targetDef,
                                                           const DataBinding::Binding& binding) final;
    void ExtractAllProperties(DataBinding::DependencyPropertyDefinitionVector& rProperties) final;

  private:
    //! @brief Work out where the content has to be for the window MakeVisible was asked for to be inside the view
    //! @param contentOffsetPx where the content was just arranged at
    //! @return true if the content has to move, to rOffsetPx
    bool TryCalcMakeVisibleOffset(const std::shared_ptr<BaseWindow>& window, const PxSize2D viewSizePx, const PxSize2D contentSizePx,
                                  const PxPoint2 contentOffsetPx, PxPoint2& rOffsetPx) const;
  };
}


#endif
