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


#include <FslBase/NumericCast.hpp>
#include <FslDataBinding/Base/Object/DependencyObjectHelper.hpp>
#include <FslDataBinding/Base/Object/DependencyPropertyDefinitionVector.hpp>
#include <FslDataBinding/Base/Property/DependencyPropertyDefinitionFactory.hpp>
#include <FslSimpleUI/Base/Control/SelectorLabel.hpp>
#include <FslSimpleUI/Base/PropertyTypeFlags.hpp>
#include <algorithm>
#include <utility>

namespace Fsl::UI
{
  using TClass = SelectorLabel;
  using TDef = DataBinding::DependencyPropertyDefinition;
  using TFactory = DataBinding::DependencyPropertyDefinitionFactory;

  TDef TClass::PropertySelectedIndex = TFactory::Create<uint32_t, TClass, &TClass::GetSelectedIndex, &TClass::SetSelectedIndex>("SelectedIndex");
}

namespace Fsl::UI
{
  SelectorLabel::SelectorLabel(const std::shared_ptr<WindowContext>& context)
    : LabelBase(context)
  {
  }


  bool SelectorLabel::SetEntries(std::vector<std::string> entries)
  {
    if (entries == m_entries)
    {
      return false;
    }
    m_entries = std::move(entries);
    m_propertySelectedIndex.Set(ThisDependencyObject(), ClampIndex(m_propertySelectedIndex.Get()));
    // The text that is shown can be another one now, and the size of the label depends on every entry
    DoSetContent(GetContent());
    return true;
  }


  bool SelectorLabel::SetSelectedIndex(const uint32_t value)
  {
    const bool changed = m_propertySelectedIndex.Set(ThisDependencyObject(), ClampIndex(value));
    if (changed)
    {
      DoSetContent(GetContent());
    }
    return changed;
  }


  PxSize2D SelectorLabel::MeasureOverride(const PxAvailableSize& availableSizePx)
  {
    // The entry that is shown, then the others: the label is as large as the largest of them
    PxSize2D sizePx = base_type::MeasureOverride(availableSizePx);
    for (const auto& entry : m_entries)
    {
      sizePx.SetMax(DoMeasureRenderedString(entry));
    }
    return sizePx;
  }


  DataBinding::DataBindingInstanceHandle SelectorLabel::TryGetPropertyHandleNow(const DataBinding::DependencyPropertyDefinition& sourceDef)
  {
    const auto res = DataBinding::DependencyObjectHelper::TryGetPropertyHandle(
      this, ThisDependencyObject(), sourceDef, DataBinding::PropLinkRefs(PropertySelectedIndex, m_propertySelectedIndex));
    return res.IsValid() ? res : base_type::TryGetPropertyHandleNow(sourceDef);
  }


  DataBinding::PropertySetBindingResult SelectorLabel::TrySetBindingNow(const DataBinding::DependencyPropertyDefinition& targetDef,
                                                                        const DataBinding::Binding& binding)
  {
    const auto res = DataBinding::DependencyObjectHelper::TrySetBinding(this, ThisDependencyObject(), targetDef, binding,
                                                                        DataBinding::PropLinkRefs(PropertySelectedIndex, m_propertySelectedIndex));
    return res != DataBinding::PropertySetBindingResult::NotFound ? res : base_type::TrySetBindingNow(targetDef, binding);
  }


  void SelectorLabel::ExtractAllProperties(DataBinding::DependencyPropertyDefinitionVector& rProperties)
  {
    base_type::ExtractAllProperties(rProperties);
    rProperties.push_back(PropertySelectedIndex);
  }


  uint32_t SelectorLabel::ClampIndex(const uint32_t index) const noexcept
  {
    const auto lastIndex = static_cast<uint32_t>(m_entries.empty() ? 0u : (m_entries.size() - 1u));
    return std::min(index, lastIndex);
  }
}
