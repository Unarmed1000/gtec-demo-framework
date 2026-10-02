#ifndef FSLSIMPLEUI_BASE_CONTROL_SLIDERBASE_HPP
#define FSLSIMPLEUI_BASE_CONTROL_SLIDERBASE_HPP
/****************************************************************************************************************************************************
 * Copyright 2020, 2022-2023 NXP
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

#include <FslBase/Math/Pixel/PxRectangle.hpp>
#include <FslBase/Math/ThicknessF.hpp>
#include <FslDataBinding/Base/Object/DependencyObjectHelper.hpp>
#include <FslDataBinding/Base/Object/DependencyPropertyDefinitionVector.hpp>
#include <FslDataBinding/Base/Property/DependencyPropertyDefinition.hpp>
#include <FslDataBinding/Base/Property/DependencyPropertyDefinitionFactory.hpp>
#include <FslDataBinding/Base/Property/TypedDependencyProperty.hpp>
#include <FslSimpleUI/Base/BaseWindow.hpp>
#include <FslSimpleUI/Base/Control/Logic/SliderClickLogic.hpp>
#include <FslSimpleUI/Base/Control/Logic/SliderLogic.hpp>
#include <FslSimpleUI/Base/Control/SliderContentChangedReason.hpp>
#include <FslSimpleUI/Base/Event/WindowContentChangedEvent.hpp>
#include <FslSimpleUI/Base/Event/WindowEventPool.hpp>
#include <FslSimpleUI/Base/Event/WindowInputClickEvent.hpp>
#include <FslSimpleUI/Base/Layout/LayoutOrientation.hpp>
#include <FslSimpleUI/Base/LayoutDirection.hpp>
#include <FslSimpleUI/Base/PropertyTypeFlags.hpp>
#include <FslSimpleUI/Base/WindowContext.hpp>
#include <algorithm>

namespace Fsl::UI
{
  template <typename T>
  class SliderBase : public BaseWindow
  {
    using base_type = BaseWindow;

  public:
    using value_type = T;

  protected:
    // NOLINTNEXTLINE(readability-identifier-naming)
    const std::shared_ptr<WindowContext> m_windowContext;

  private:
    SliderLogic<value_type> m_logic;
    //! What the slider holds of a click, and who owns the gesture (see SliderClickLogic)
    SliderClickLogic<value_type> m_clickLogic;
    typename DataBinding::TypedDependencyProperty<LayoutOrientation> m_propertyOrientation{LayoutOrientation::Horizontal};
    typename DataBinding::TypedDependencyProperty<LayoutDirection> m_propertyDirection{LayoutDirection::NearToFar};
    typename DataBinding::TypedDependencyProperty<value_type> m_propertyValue;

  public:
    explicit SliderBase(const std::shared_ptr<WindowContext>& context)
      : BaseWindow(context)
      , m_windowContext(context)
      , m_logic(SliderConstrainedValue<value_type>(value_type(0), value_type(0), value_type(100)))
    {
      SyncLogicAndProperty();
    }

    //! @brief Check if the slider is being dragged or not
    bool IsDragging() const
    {
      return m_logic.IsDragging();
    }

    LayoutOrientation GetOrientation() const noexcept
    {
      return m_propertyOrientation.Get();
    }

    LayoutDirection GetDirection() const noexcept
    {
      return m_propertyDirection.Get();
    }

    bool IsEnabled() const
    {
      return m_logic.IsEnabled();
    }

    value_type GetMinValue() const
    {
      return m_logic.Min();
    }

    value_type GetMaxValue() const
    {
      return m_logic.Max();
    }

    value_type GetTickFrequency() const
    {
      return m_logic.GetTickFrequency();
    }

    bool SetOrientation(const LayoutOrientation value)
    {
      const bool changed = m_propertyOrientation.Set(ThisDependencyObject(), value);
      if (changed)
      {
        PropertyUpdated(PropertyType::Layout);
      }
      return changed;
    }

    bool SetDirection(const LayoutDirection value)
    {
      const bool changed = m_propertyDirection.Set(ThisDependencyObject(), value);
      if (changed)
      {
        PropertyUpdated(PropertyType::Layout);
      }
      return changed;
    }

    //! @brief When disabled the slider can not be edited by the user
    bool SetEnabled(const bool enabled)
    {
      auto flags = m_logic.SetEnabled(enabled);
      if (!enabled)
      {
        // The logic gave up a drag, a click on the bar that is held is given up as well
        m_clickLogic.Reset();
      }
      SyncLogicAndProperty();
      if (SliderResultFlagsUtil::IsFlagged(flags, SliderResultFlags::Completed))
      {
        PropertyUpdated(PropertyType::Other);
        if (SliderResultFlagsUtil::IsFlagged(flags, SliderResultFlags::DragCancelled))
        {
          DoSendWindowContentChangedEvent(SliderContentChangedReason::DragCanceled);
        }
        return true;
      }
      return false;
    }

    value_type GetValue() const
    {
      return m_logic.GetValue();
    }

    bool SetValue(const value_type value)
    {
      if (!IsReadOnly() && m_logic.SetValue(value))
      {
        SyncLogicAndProperty();
        PropertyUpdated(PropertyType::ContentDraw);
        DoSendWindowContentChangedEvent(SliderContentChangedReason::Set);
        return true;
      }
      return false;
    }

    bool AddValue(const value_type value)
    {
      const auto range = m_logic.Max() - m_logic.Min();
      if (value <= range && m_logic.GetValue() <= (m_logic.Max() - value))
      {
        return SetValue(m_logic.GetValue() + value);
      }
      return SetValue(m_logic.Max());
    }

    bool SubValue(const value_type value)
    {
      const auto range = m_logic.Max() - m_logic.Min();
      if (value <= range && m_logic.GetValue() >= (m_logic.Min() + value))
      {
        return SetValue(m_logic.GetValue() - value);
      }
      return SetValue(m_logic.Min());
    }

    bool SetRange(const value_type min, const value_type max)
    {
      if (m_logic.SetRange(min, max))
      {
        SyncLogicAndProperty();
        PropertyUpdated(PropertyType::Other);
        DoSendWindowContentChangedEvent(SliderContentChangedReason::Set);
        return true;
      }
      return false;
    }

    bool SetTickFrequency(const value_type tickFrequency)
    {
      if (m_logic.SetTickFrequency(tickFrequency))
      {
        SyncLogicAndProperty();
        PropertyUpdated(PropertyType::Other);
        DoSendWindowContentChangedEvent(SliderContentChangedReason::Set);
        return true;
      }
      return false;
    }

  protected:
    //! @brief The area of the cursor (the handle) relative to the slider. A press inside it begins a drag, a press elsewhere on the
    //!        bar sets the value when it is released.
    //! @note  It answers a input question, so it must be calculated from the current state of the slider and not be a value that
    //!        was stored while drawing or arranging: a drag moves the cursor without a new layout.
    [[nodiscard]] virtual PxRectangle CalcCursorGrabRectanglePx() const = 0;

    void OnClickInput(const std::shared_ptr<WindowInputClickEvent>& theEvent) override
    {
      SliderClickInput input;
      input.State = theEvent->GetState();
      input.IsRepeat = theEvent->IsRepeat();
      input.PositionPx = PointFromScreen(theEvent->GetScreenPosition());
      input.BarRectanglePx = PxRectangle(PxPoint2(), RenderSizePx());
      input.CursorGrabRectanglePx = CalcCursorGrabRectanglePx();
      input.IsHorizontal = (m_propertyOrientation.Get() == LayoutOrientation::Horizontal);
      input.IsReadOnly = IsReadOnly();

      const SliderClickResult<value_type> result = m_clickLogic.Process(m_logic, input);
      switch (result.Action)
      {
      case SliderClickAction::DragBegin:
        SyncLogicAndProperty();
        DoSendWindowContentChangedEvent(SliderContentChangedReason::DragBegin);
        break;
      case SliderClickAction::Drag:
        SyncLogicAndProperty();
        DoSendWindowContentChangedEvent(SliderContentChangedReason::Drag);
        break;
      case SliderClickAction::DragEnd:
        SyncLogicAndProperty();
        DoSendWindowContentChangedEvent(SliderContentChangedReason::DragEnd);
        break;
      case SliderClickAction::DragCanceled:
        SyncLogicAndProperty();
        DoSendWindowContentChangedEvent(SliderContentChangedReason::DragCanceled);
        break;
      case SliderClickAction::SetValue:
        SetValue(result.Value);
        break;
      case SliderClickAction::NoAction:
      default:
        break;
      }

      // The status tells a ScrollViewer the slider is in if it may take the gesture: a drag of the cursor is claimed, a click on the
      // bar is only handled (the ScrollViewer can turn it into a scroll)
      switch (result.Status)
      {
      case EventHandlingStatus::Claimed:
        theEvent->Claimed();
        break;
      case EventHandlingStatus::Handled:
        theEvent->Handled();
        break;
      case EventHandlingStatus::Unhandled:
      default:
        break;
      }
    }

    PxValue GetCursorPositionPx() const
    {
      return m_logic.GetPositionPx();
    }

    const SliderPixelSpanInfo& GetRenderInfo() const
    {
      return m_logic.GetSpanInfo();
    }

    //! @brief Allow the inheriting class to set the render information
    inline void SetSpanInfo(const SliderPixelSpanInfo& spanInfo)
    {
      m_logic.SetSpanInfo(spanInfo);
      SyncLogicAndProperty();
    }

  private:
    void DoSendWindowContentChangedEvent(SliderContentChangedReason reason)
    {
      if (IsReadyToSendEvents())
      {
        SendEvent(GetEventPool()->AcquireWindowContentChangedEvent(0, static_cast<int32_t>(m_logic.GetValue()), static_cast<int32_t>(reason)));
      }
    }

    bool IsReadOnly() const noexcept
    {
      return m_propertyValue.IsReadOnly(ThisDependencyObject());
    }

    void SyncLogicAndProperty()
    {
      if (!IsReadOnly() && m_logic.GetValue() != m_propertyValue.Get())
      {
        m_propertyValue.Set(ThisDependencyObject(), m_logic.GetValue(), DataBinding::PropertyChangeReason::Refresh);
      }
    }


  public:
    // NOLINTNEXTLINE(readability-identifier-naming)
    inline static typename DataBinding::DependencyPropertyDefinition PropertyOrientation =
      DataBinding::DependencyPropertyDefinitionFactory::Create<LayoutOrientation, SliderBase, &SliderBase::GetOrientation,
                                                               &SliderBase::SetOrientation>("Orientation");
    // NOLINTNEXTLINE(readability-identifier-naming)
    inline static typename DataBinding::DependencyPropertyDefinition PropertyDirection =
      DataBinding::DependencyPropertyDefinitionFactory::Create<LayoutDirection, SliderBase, &SliderBase::GetDirection, &SliderBase::SetDirection>(
        "Direction");
    // NOLINTNEXTLINE(readability-identifier-naming)
    inline static typename DataBinding::DependencyPropertyDefinition PropertyValue =
      DataBinding::DependencyPropertyDefinitionFactory::Create<value_type, SliderBase, &SliderBase::GetValue, &SliderBase::SetValue>("Value");

  protected:
    DataBinding::DataBindingInstanceHandle TryGetPropertyHandleNow(const DataBinding::DependencyPropertyDefinition& sourceDef) final
    {
      auto res = DataBinding::DependencyObjectHelper::TryGetPropertyHandle(
        this, ThisDependencyObject(), sourceDef, DataBinding::PropLinkRefs(PropertyOrientation, m_propertyOrientation),
        DataBinding::PropLinkRefs(PropertyDirection, m_propertyDirection), DataBinding::PropLinkRefs(SliderBase::PropertyValue, m_propertyValue));
      return res.IsValid() ? res : base_type::TryGetPropertyHandleNow(sourceDef);
    }

    DataBinding::PropertySetBindingResult TrySetBindingNow(const DataBinding::DependencyPropertyDefinition& targetDef,
                                                           const DataBinding::Binding& binding) final
    {
      auto res = DataBinding::DependencyObjectHelper::TrySetBinding(this, ThisDependencyObject(), targetDef, binding,
                                                                    DataBinding::PropLinkRefs(SliderBase::PropertyOrientation, m_propertyOrientation),
                                                                    DataBinding::PropLinkRefs(SliderBase::PropertyDirection, m_propertyDirection),
                                                                    DataBinding::PropLinkRefs(SliderBase::PropertyValue, m_propertyValue));
      return res != DataBinding::PropertySetBindingResult::NotFound ? res : base_type::TrySetBindingNow(targetDef, binding);
    }

    void ExtractAllProperties(DataBinding::DependencyPropertyDefinitionVector& rProperties) final
    {
      base_type::ExtractAllProperties(rProperties);
      rProperties.push_back(SliderBase::PropertyOrientation);
      rProperties.push_back(SliderBase::PropertyDirection);
      rProperties.push_back(SliderBase::PropertyValue);
    }
  };
}

#endif
