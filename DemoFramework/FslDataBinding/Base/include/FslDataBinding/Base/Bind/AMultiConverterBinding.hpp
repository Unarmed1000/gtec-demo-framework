#ifndef FSLDATABINDING_BASE_BIND_AMULTICONVERTERBINDING_HPP
#define FSLDATABINDING_BASE_BIND_AMULTICONVERTERBINDING_HPP
/****************************************************************************************************************************************************
 * Copyright 2022 NXP
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

#include <FslBase/Span/ReadOnlySpan.hpp>
#include <FslBase/Span/Span.hpp>
#include <FslDataBinding/Base/Bind/IMultiBinding.hpp>
#include <FslDataBinding/Base/Bind/PropertyTypeInfo.hpp>
#include <FslDataBinding/Base/Internal/PropertyGetInfo.hpp>
#include <FslDataBinding/Base/Internal/PropertySetInfo.hpp>
#include <typeindex>

namespace Fsl::DataBinding
{
  namespace Internal
  {
    class IPropertyMethods;
  }


  class AMultiConverterBinding : public IMultiBinding
  {
  public:
    BindingType GetBindingType() const noexcept final
    {
      return BindingType::AMultiConverterBinding;
    }

    virtual ReadOnlySpan<PropertyTypeInfo> GetSourceTypes() const = 0;
    virtual std::type_index GetTargetType() const = 0;

    virtual Internal::PropertySetResult Convert(const Internal::PropertyMethodsImplType setPropertyMethodsImplType,
                                                Internal::IPropertyMethods* const pSet, const ReadOnlySpan<Internal::PropertyGetInfo> getters) = 0;
    //! If this is not a two-way converter this method should return false
    virtual bool TryConvertBack(Span<Internal::PropertySetResult> resultSpan, const ReadOnlySpan<Internal::PropertySetInfo> setters,
                                const Internal::PropertyGetInfo getter) = 0;
  };
}

#endif
