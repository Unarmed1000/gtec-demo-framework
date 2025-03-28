#ifndef FSLDATABINDING_UNITTEST_FSLDATABINDING_BASE_UNITTEST_UTOBSERVABLECOLLECTION_HPP
#define FSLDATABINDING_UNITTEST_FSLDATABINDING_BASE_UNITTEST_UTOBSERVABLECOLLECTION_HPP
/****************************************************************************************************************************************************
 * Copyright 2022, 2024 NXP
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

#include <FslDataBinding/Base/Object/ObservableDataSourceObject.hpp>
#include <FslDataBinding/Base/Property/TypedReadOnlyDependencyProperty.hpp>
#include <FslDataBinding/Base/ScopedDataSourceObject.hpp>

namespace Fsl
{
  class UTObservableCollection : public DataBinding::ObservableDataSourceObject
  {
    using prop0_type = uint32_t;
    using prop1_type = float;
    DataBinding::TypedReadOnlyDependencyProperty<prop0_type> m_property0;
    DataBinding::TypedReadOnlyDependencyProperty<prop1_type> m_property1;

  public:
    explicit UTObservableCollection(const std::shared_ptr<DataBinding::DataBindingService>& dataBinding);
    bool MarkAsChanged();

    uint32_t GetProperty0Value() const noexcept
    {
      return m_property0.Get();
    }

    float GetProperty1Value() const noexcept
    {
      return m_property1.Get();
    }

    bool SetProperty0Value(const uint32_t value);
    bool SetProperty1Value(const float value);


    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition Property0;
    // NOLINTNEXTLINE(readability-identifier-naming)
    static DataBinding::DependencyPropertyDefinition Property1;

  protected:
    DataBinding::DataBindingInstanceHandle TryGetPropertyHandleNow(const DataBinding::DependencyPropertyDefinition& sourceDef) final;
  };
}

#endif
