#ifndef FSLSIMPLEUI_BASE_CONTROL_LOGIC_SCROLLMAKEVISIBLEMATH_HPP
#define FSLSIMPLEUI_BASE_CONTROL_LOGIC_SCROLLMAKEVISIBLEMATH_HPP
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

#include <FslBase/Math/Pixel/PxSize1D.hpp>
#include <FslBase/Math/Pixel/PxValue.hpp>
#include <algorithm>

namespace Fsl::UI::ScrollMakeVisibleMath
{
  //! @brief The distance the content has to move along one axis for a part of it to be inside the view.
  //! @param viewSizePx the size of the view, which begins at zero
  //! @param positionPx where the part begins, relative to the start of the view
  //! @param sizePx the size of the part
  //! @return zero for a part that is inside the view. Else the shortest distance that brings it in: its start to the start of the view
  //!         for a part that begins before the view, its end to the end of the view for one that ends after it. A part that is larger
  //!         than the view is shown from its start.
  [[nodiscard]] constexpr PxValue CalcMoveDistancePx(const PxSize1D viewSizePx, const PxValue positionPx, const PxSize1D sizePx) noexcept
  {
    if (positionPx.Value < 0)
    {
      return PxValue(-positionPx.Value);
    }
    const int32_t pastTheEndPx = (positionPx.Value + sizePx.RawValue()) - viewSizePx.RawValue();
    if (pastTheEndPx > 0)
    {
      // Never further than the start of the part, the rest of a part that does not fit stays past the end
      return PxValue(-std::min(pastTheEndPx, positionPx.Value));
    }
    return {};
  }

  //! @brief Keep the place of the content inside what can be scrolled to along one axis.
  //! @param offsetPx the place of the content: zero at its start, the size of the view minus the size of the content at its end
  //! @return the offset, no further than the two ends. Zero for content that fits in the view.
  [[nodiscard]] constexpr PxValue ClampOffsetPx(const PxValue offsetPx, const PxSize1D viewSizePx, const PxSize1D contentSizePx) noexcept
  {
    const int32_t endPx = std::min(viewSizePx.RawValue() - contentSizePx.RawValue(), 0);
    return PxValue(std::clamp(offsetPx.Value, endPx, 0));
  }
}

#endif
