#ifndef FSLBASE_MATH_PIXEL_PXVECTOR2_HPP
#define FSLBASE_MATH_PIXEL_PXVECTOR2_HPP
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

#include <FslBase/BasicTypes.hpp>
#include <FslBase/Math/Pixel/PxSize1DF.hpp>
#include <FslBase/Math/Pixel/PxValueF.hpp>

namespace Fsl
{
  struct PxVector2
  {
    using value_type = PxValueF;
    using raw_value_type = value_type::raw_value_type;

    PxValueF X{0};
    PxValueF Y{0};

    constexpr PxVector2() noexcept = default;

    constexpr PxVector2(const value_type x, const value_type y) noexcept
      : X(x)
      , Y(y)
    {
    }

    constexpr PxVector2(const value_type x, const PxSize1DF y) noexcept
      : X(x)
      , Y(y.Value())
    {
    }

    constexpr PxVector2(const PxSize1DF x, const value_type y) noexcept
      : X(x.Value())
      , Y(y)
    {
    }

    constexpr PxVector2(const PxSize1DF x, const PxSize1DF y) noexcept
      : X(x.Value())
      , Y(y.Value())
    {
    }

    //! @brief Calculates the length of the vector squared.
    constexpr float LengthSquared() const
    {
      return (X.Value * X.Value) + (Y.Value * Y.Value);
    }


    constexpr PxVector2& operator+=(const PxVector2 arg) noexcept
    {
      X += arg.X;
      Y += arg.Y;
      return *this;
    }

    constexpr PxVector2& operator-=(const PxVector2 arg) noexcept
    {
      X -= arg.X;
      Y -= arg.Y;
      return *this;
    }

    constexpr PxVector2& operator*=(const PxVector2 arg) noexcept
    {
      X *= arg.X;
      Y *= arg.Y;
      return *this;
    }

    constexpr PxVector2& operator*=(const value_type arg) noexcept
    {
      X *= arg;
      Y *= arg;
      return *this;
    }

    constexpr PxVector2& operator/=(const value_type arg)
    {
      X /= arg;
      Y /= arg;
      return *this;
    }

    constexpr bool operator==(const PxVector2 rhs) const noexcept
    {
      return X == rhs.X && Y == rhs.Y;
    }

    constexpr bool operator!=(const PxVector2 rhs) const noexcept
    {
      return X != rhs.X || Y != rhs.Y;
    }

    // @brief Returns a PxVector2 with all components being zero (0, 0)
    static constexpr PxVector2 Zero() noexcept
    {
      return {};
    }


    static constexpr PxVector2 Min(const PxVector2 val0, const PxVector2 val1) noexcept
    {
      return {value_type::Min(val0.X, val1.X), value_type::Min(val0.Y, val1.Y)};
    }

    static constexpr PxVector2 Max(const PxVector2 val0, const PxVector2 val1) noexcept
    {
      return {value_type::Max(val0.X, val1.X), value_type::Max(val0.Y, val1.Y)};
    }

    static constexpr PxVector2 Create(const raw_value_type x, const raw_value_type y) noexcept
    {
      return {value_type(x), value_type(y)};
    }
  };


  inline constexpr PxVector2 operator+(const PxVector2 lhs, const PxVector2 rhs) noexcept
  {
    return {lhs.X + rhs.X, lhs.Y + rhs.Y};
  }


  inline constexpr PxVector2 operator-(const PxVector2 lhs, const PxVector2 rhs) noexcept
  {
    return {lhs.X - rhs.X, lhs.Y - rhs.Y};
  }


  inline constexpr PxVector2 operator*(const PxVector2 lhs, const PxVector2 rhs) noexcept
  {
    return {lhs.X * rhs.X, lhs.Y * rhs.Y};
  }

  inline constexpr PxVector2 operator*(const PxVector2 lhs, const PxVector2::value_type rhs) noexcept
  {
    return {lhs.X * rhs, lhs.Y * rhs};
  }

  inline constexpr PxVector2 operator*(const PxVector2::value_type lhs, const PxVector2 rhs) noexcept
  {
    return rhs * lhs;
  }

  inline constexpr PxVector2 operator/(const PxVector2 lhs, const PxVector2::value_type rhs)
  {
    return {lhs.X / rhs, lhs.Y / rhs};
  }

}

#endif
