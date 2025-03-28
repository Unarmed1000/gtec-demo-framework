#ifndef FSLBASE_MATH_PIXEL_TYPECONVERTER_MATH_HPP
#define FSLBASE_MATH_PIXEL_TYPECONVERTER_MATH_HPP
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

#include <FslBase/Math/Extent2D.hpp>
#include <FslBase/Math/Offset2D.hpp>
#include <FslBase/Math/Pixel/TypeConverter.hpp>
#include <FslBase/Math/Point2.hpp>
#include <FslBase/Math/Rect.hpp>
#include <FslBase/Math/Rectangle.hpp>
#include <FslBase/Math/Vector2.hpp>
#include <cassert>
#include <cmath>

namespace Fsl::TypeConverter
{
  // The in this file are responsible for converting
  // PX     -> normal
  // normal -> PX

  // --- PxSize2D

  template <>
  constexpr inline PxSize2D To<PxSize2D, Point2>(const Point2& value)
  {
    static_assert(std::is_same<PxSize2D::raw_value_type, Point2::value_type>::value, "we expect the types to be equal");
    return PxSize2D::Create(value.X, value.Y);
  }

  // --- PxVector2

  template <>
  constexpr inline PxVector2 To<PxVector2, Vector2>(const Vector2& value)
  {
    return PxVector2::Create(value.X, value.Y);
  }

  // --- Vector2

  template <>
  constexpr inline Vector2 To<Vector2, PxVector2>(const PxVector2& value)
  {
    return {value.X.Value, value.Y.Value};
  }

  // --- Point2

  template <>
  constexpr inline Point2 To<Point2, PxExtent2D>(const PxExtent2D& value)
  {
    return {NumericCast<Point2::value_type>(value.Width.Value), NumericCast<Point2::value_type>(value.Height.Value)};
  }


  // -----------------------------------------------------------------------------------------------------------------------------------------------


  // --- Extend2D

  template <>
  constexpr inline Extent2D UncheckedTo<Extent2D, PxExtent2D>(const PxExtent2D& value) noexcept
  {
    static_assert(std::is_same<Extent2D::value_type, PxExtent2D::raw_value_type>::value, "we expect the types to be equal");
    return {value.Width.Value, value.Height.Value};
  }

  // --- Offset2D

  template <>
  constexpr inline Offset2D UncheckedTo<Offset2D, PxPoint2>(const PxPoint2& value) noexcept
  {
    static_assert(std::is_same<Offset2D::value_type, PxPoint2::raw_value_type>::value, "we expect the types to be equal");
    return {value.X.Value, value.Y.Value};
  }

  // --- Point2

  template <>
  constexpr inline Point2 UncheckedTo<Point2, PxPoint2>(const PxPoint2& value) noexcept
  {
    static_assert(std::is_same<Point2::value_type, PxPoint2::raw_value_type>::value, "we expect the types to be equal");
    return {value.X.Value, value.Y.Value};
  }

  template <>
  constexpr inline Point2 UncheckedTo<Point2, PxSize2D>(const PxSize2D& value) noexcept
  {
    static_assert(std::is_same<Point2::value_type, PxSize2D::raw_value_type>::value, "we expect the types to be equal");
    return {value.RawWidth(), value.RawHeight()};
  }

  template <>
  constexpr inline Point2 UncheckedTo<Point2, PxExtent2D>(const PxExtent2D& value) noexcept
  {
    return {UncheckedNumericCast<Point2::value_type>(value.Width.Value), UncheckedNumericCast<Point2::value_type>(value.Height.Value)};
  }

  // --- PxClipRectangle

  //! @brief Convert a rect to a PxClipRectangle

  template <>
  inline PxClipRectangle UncheckedChangeTo<PxClipRectangle, Rect>(const Rect& value)
  {
    // This should always be the case for a Rect (so we only assert check it)
    assert(value.Left() <= value.Right() && value.Top() <= value.Bottom());
    return PxClipRectangle::UncheckedCreateFromLeftTopRightBottom(
      UncheckedChangeTo<PxClipRectangle::raw_value_type>(value.Left()), UncheckedChangeTo<PxClipRectangle::raw_value_type>(value.Top()),
      UncheckedChangeTo<PxClipRectangle::raw_value_type>(value.Right()), UncheckedChangeTo<PxClipRectangle::raw_value_type>(value.Bottom()));
  }

  // --- PxAreaRectangleF

  template <>
  constexpr inline PxAreaRectangleF UncheckedTo<PxAreaRectangleF, Rect>(const Rect& value) noexcept
  {
    static_assert(std::is_same<PxAreaRectangleF::raw_value_type, Rect::value_type>::value, "we expect the types to be equal");

    // This should always be the case for a Rect (so we only assert check it)
    assert(value.Left() <= value.Right() && value.Top() <= value.Bottom());
    return PxAreaRectangleF::UncheckedCreateFromLeftTopRightBottom(value.Left(), value.Top(), value.Right(), value.Bottom());
  }

  // --- PxExtent2D

  template <>
  constexpr inline PxExtent2D UncheckedTo<PxExtent2D, Extent2D>(const Extent2D& value) noexcept
  {
    static_assert(std::is_same<PxExtent2D::raw_value_type, Extent2D::value_type>::value, "we expect the types to be equal");
    return PxExtent2D::Create(value.Width, value.Height);
  }

  template <>
  constexpr inline PxExtent2D UncheckedTo<PxExtent2D, Point2>(const Point2& value) noexcept
  {
    return PxExtent2D::Create(UncheckedNumericCast<PxExtent2D::raw_value_type>(value.X), UncheckedNumericCast<PxExtent2D::raw_value_type>(value.Y));
  }

  // --- PxPoint2

  template <>
  constexpr inline PxPoint2 UncheckedTo<PxPoint2, Point2>(const Point2& value) noexcept
  {
    static_assert(std::is_same<PxPoint2::raw_value_type, Point2::value_type>::value, "we expect the types to be equal");
    return PxPoint2::Create(value.X, value.Y);
  }

  template <>
  inline PxPoint2 UncheckedChangeTo<PxPoint2, Vector2>(const Vector2& value)
  {
    return PxPoint2::Create(UncheckedChangeTo<PxPoint2::raw_value_type>(value.X), UncheckedChangeTo<PxPoint2::raw_value_type>(value.Y));
  }

  // --- PxRectangle

  //! @brief Convert a rect to a PxRectangle
  template <>
  inline PxRectangle UncheckedChangeTo<PxRectangle, Rect>(const Rect& value)
  {
    // This should always be the case for a Rect (so we only assert check it)
    assert(value.Left() <= value.Right() && value.Top() <= value.Bottom());
    return PxRectangle::UncheckedCreateFromLeftTopRightBottom(
      UncheckedChangeTo<PxRectangle::raw_value_type>(value.Left()), UncheckedChangeTo<PxRectangle::raw_value_type>(value.Top()),
      UncheckedChangeTo<PxRectangle::raw_value_type>(value.Right()), UncheckedChangeTo<PxRectangle::raw_value_type>(value.Bottom()));
  }

  // -- PxRectangle

  //! @brief Convert a rectangle to a PxRectangle
  template <>
  constexpr inline PxRectangle UncheckedTo<PxRectangle, Rectangle>(const Rectangle& value) noexcept
  {
    // This should always be the case for a Rectangle (so we only assert check it)
    assert(value.Left() <= value.Right() && value.Top() <= value.Bottom());
    return PxRectangle::UncheckedCreate(
      UncheckedNumericCast<PxRectangle::raw_value_type>(value.X()), UncheckedNumericCast<PxRectangle::raw_value_type>(value.Y()),
      UncheckedNumericCast<PxRectangle::raw_value_type>(value.Width()), UncheckedNumericCast<PxRectangle::raw_value_type>(value.Height()));
  }

  // -- PxRectangleU16

  //! @brief Convert a rectangle to a PxRectangle16 (if the value.Location is negative it will be clipped against zero)
  template <>
  constexpr inline PxRectangleU16 UncheckedTo<PxRectangleU16, Rectangle>(const Rectangle& value) noexcept
  {
    // This should always be the case for a Rectangle (so we only assert check it)
    assert(value.Left() <= value.Right() && value.Top() <= value.Bottom());
    return PxRectangleU16::UncheckedCreateFromLeftTopRightBottom(
      UncheckedNumericCast<PxRectangleU16::raw_value_type>(value.Left()), UncheckedNumericCast<PxRectangleU16::raw_value_type>(value.Top()),
      UncheckedNumericCast<PxRectangleU16::raw_value_type>(value.Right()), UncheckedNumericCast<PxRectangleU16::raw_value_type>(value.Bottom()));
  }

  // -- PxRectangleU32

  //! @brief Convert a rectangle to a PxRectangle (if the value.Location is negative it will be clipped against zero)
  template <>
  constexpr inline PxRectangleU32 UncheckedTo<PxRectangleU32, Rectangle>(const Rectangle& value) noexcept
  {
    // This should always be the case for a Rectangle (so we only assert check it)
    assert(value.Left() <= value.Right() && value.Top() <= value.Bottom());
    return PxRectangleU32::UncheckedCreateFromLeftTopRightBottom(
      UncheckedNumericCast<PxRectangleU32::raw_value_type>(value.Left()), UncheckedNumericCast<PxRectangleU32::raw_value_type>(value.Top()),
      UncheckedNumericCast<PxRectangleU32::raw_value_type>(value.Right()), UncheckedNumericCast<PxRectangleU32::raw_value_type>(value.Bottom()));
  }

  // --- PxSize2D

  template <>
  constexpr inline PxSize2D UncheckedTo<PxSize2D, Point2>(const Point2& value) noexcept
  {
    static_assert(std::is_same<PxSize2D::raw_value_type, Point2::value_type>::value, "we expect the types to be equal");
    return PxSize2D::Create(value.X, value.Y);
  }

  template <>
  inline PxSize2D UncheckedChangeTo<PxSize2D, Vector2>(const Vector2& value)
  {
    return PxSize2D::Create(UncheckedChangeTo<PxPoint2::raw_value_type>(value.X), UncheckedChangeTo<PxPoint2::raw_value_type>(value.Y));
  }

  // --- Rect

  template <>
  constexpr inline Rect UncheckedTo<Rect, PxRectangle>(const PxRectangle& value) noexcept
  {
    return Rect::FromLeftTopRightBottom(static_cast<float>(value.RawLeft()), static_cast<float>(value.RawTop()), static_cast<float>(value.RawRight()),
                                        static_cast<float>(value.RawBottom()));
  }

  // --- Vector2

  template <>
  constexpr inline Vector2 UncheckedTo<Vector2, PxPoint2>(const PxPoint2& value) noexcept
  {
    return {static_cast<float>(value.X.Value), static_cast<float>(value.Y.Value)};
  }

  template <>
  constexpr inline Vector2 UncheckedTo<Vector2, PxSize2D>(const PxSize2D& value) noexcept
  {
    return {static_cast<float>(value.RawWidth()), static_cast<float>(value.RawHeight())};
  }

  template <>
  constexpr inline Vector2 UncheckedTo<Vector2, PxExtent2D>(const PxExtent2D& value) noexcept
  {
    return {static_cast<float>(value.Width.Value), static_cast<float>(value.Height.Value)};
  }

  template <>
  constexpr inline Vector2 UncheckedTo<Vector2, PxVector2>(const PxVector2& value) noexcept
  {
    return {value.X.Value, value.Y.Value};
  }

}

#endif
