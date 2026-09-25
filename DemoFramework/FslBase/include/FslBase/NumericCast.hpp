#ifndef FSLBASE_NUMERICCAST_HPP
#define FSLBASE_NUMERICCAST_HPP
/****************************************************************************************************************************************************
 * Copyright 2020 NXP
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

#include <FslBase/Attributes.hpp>
#include <FslBase/Exceptions.hpp>
#include <FslBase/UncheckedNumericCast.hpp>
#include <type_traits>

namespace Fsl
{
  //! This is a safe way to convert between types as any issues will throw a ConversionException
  //! If you are after a alternative to static_cast then use FastConvert instead as it will do no checking in release builds.
  //!
  //! The main goal of this file is to provide 'checked' conversion between the C++ fundamental data types
  //! https://en.cppreference.com/w/cpp/language/types
  //!
  //! The main methods are
  //! - To<type>        Convert the input type to the given type with proper checks. This can be done as a constexpr.
  //! - ChangeTo<type>  Convert the input type to the given type with prpoer checks. This can not be done as a constexpr.
  namespace IntegralConverter
  {
    template <typename TTo, typename TFrom>
    constexpr TTo ConvertIntegral(const TFrom& value)
      requires(std::is_integral_v<TTo> && std::is_integral_v<TFrom> && std::is_signed_v<TTo> && std::is_signed_v<TFrom>)
    {
      if (value < std::numeric_limits<TTo>::min() || value > std::numeric_limits<TTo>::max())
      {
        throw ConversionException();
      }
      return static_cast<TTo>(value);
    }

    template <typename TTo, typename TFrom>
    constexpr TTo ConvertIntegral(const TFrom& value)
      requires(std::is_integral_v<TTo> && std::is_integral_v<TFrom> && std::is_signed_v<TTo> && std::is_unsigned_v<TFrom>)
    {
      if (value > static_cast<std::make_unsigned_t<TFrom>>(std::numeric_limits<TTo>::max()))
      {
        throw ConversionException();
      }
      return static_cast<TTo>(value);
    }

    template <typename TTo, typename TFrom>
    constexpr TTo ConvertIntegral(const TFrom& value)
      requires(std::is_integral_v<TTo> && std::is_integral_v<TFrom> && std::is_unsigned_v<TTo> && std::is_signed_v<TFrom>)
    {
      // coverity[result_independent_of_operands]
      if (value < 0 || (static_cast<std::make_unsigned_t<TFrom>>(value) > std::numeric_limits<TTo>::max()))
      {
        throw ConversionException();
      }
      return static_cast<TTo>(value);
    }

    template <typename TTo, typename TFrom>
    constexpr TTo ConvertIntegral(const TFrom& value)
      requires(std::is_integral_v<TTo> && std::is_integral_v<TFrom> && std::is_unsigned_v<TTo> && std::is_unsigned_v<TFrom>)
    {
      if (value > std::numeric_limits<TTo>::max())
      {
        throw ConversionException();
      }
      return static_cast<TTo>(value);
    }
  }

  template <typename TTo, typename TFrom>
  constexpr inline TTo NumericCast(const TFrom& value)
    requires(std::is_integral_v<TTo> && std::is_integral_v<TFrom>)
  {
    return IntegralConverter::ConvertIntegral<TTo>(value);
  }


  ////! Convert to a type where we can not guarantee it can be done as a constexpr
  ////! This also throws a ConversionException on failure.
  // template <typename TTo, typename TFrom>
  // inline TTo ChangeTo(const TFrom& value) = delete;

}

#endif
