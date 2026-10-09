#ifndef FSLSIMPLEUI_BASE_CONTROL_SELECTORLABEL_HPP
#define FSLSIMPLEUI_BASE_CONTROL_SELECTORLABEL_HPP
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


#include <FslBase/Span/ReadOnlySpan.hpp>
#include <FslBase/Span/SpanUtil_Vector.hpp>
#include <FslBase/String/StringViewLite.hpp>
#include <FslSimpleUI/Base/Control/LabelBase.hpp>
#include <string>
#include <vector>

namespace Fsl::UI
{
  //! @brief A label that shows one of a set of texts and is as large as the largest of them. So the layout it is part of does not
  //!        change when another text is selected.
  class SelectorLabel final : public LabelBase
  {
    using base_type = LabelBase;

    std::vector<std::string> m_entries;
    DataBinding::TypedDependencyProperty<uint32_t> m_propertySelectedIndex;

  public:
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition PropertySelectedIndex;

    explicit SelectorLabel(const std::shared_ptr<WindowContext>& context);

    ReadOnlySpan<std::string> GetEntries() const noexcept
    {
      return SpanUtil::AsReadOnlySpan(m_entries);
    }

    //! @brief Set the texts the label can show. The selected index is kept, unless it is beyond the last of them.
    bool SetEntries(std::vector<std::string> entries);

    uint32_t GetSelectedIndex() const noexcept
    {
      return m_propertySelectedIndex.Get();
    }

    //! @brief Select the text that is shown. A index beyond the last entry selects the last one.
    bool SetSelectedIndex(const uint32_t value);

    //! @brief The text that is shown (empty if there are no entries)
    StringViewLite GetContent() const noexcept
    {
      const uint32_t index = m_propertySelectedIndex.Get();
      return index < m_entries.size() ? StringViewLite(m_entries[index]) : StringViewLite();
    }

  protected:
    StringViewLite DoGetContent() const final
    {
      return GetContent();
    }

    PxSize2D MeasureOverride(const PxAvailableSize& availableSizePx) final;

    DataBinding::DataBindingInstanceHandle TryGetPropertyHandleNow(const DataBinding::DependencyPropertyDefinition& sourceDef) final;
    DataBinding::PropertySetBindingResult TrySetBindingNow(const DataBinding::DependencyPropertyDefinition& targetDef,
                                                           const DataBinding::Binding& binding) final;
    void ExtractAllProperties(DataBinding::DependencyPropertyDefinitionVector& rProperties) final;

  private:
    uint32_t ClampIndex(const uint32_t index) const noexcept;
  };
}

#endif
