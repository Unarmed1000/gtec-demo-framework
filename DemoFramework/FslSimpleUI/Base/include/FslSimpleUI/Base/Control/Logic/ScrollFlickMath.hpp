#ifndef FSLSIMPLEUI_BASE_CONTROL_LOGIC_SCROLLFLICKMATH_HPP
#define FSLSIMPLEUI_BASE_CONTROL_LOGIC_SCROLLFLICKMATH_HPP
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

#include <FslBase/Math/Dp/DpPoint2F.hpp>
#include <FslBase/Math/Dp/DpValueF.hpp>

namespace Fsl::UI::ScrollFlickMath
{
  //! @brief The acceleration that brings a flick to rest: of the size of the deceleration and against the direction of the velocity.
  //! @note  With a acceleration that does not depend on the direction (the same negative value on both axes) a flick towards the end of
  //!        the content travels three times as far as one of the same speed towards its start, and a flick along one axis drifts along
  //!        the other one. Against the velocity, flicks of the same speed travel the same distance in every direction: the square of
  //!        the speed divided by twice the deceleration.
  //! @param velocityDpf the velocity of the flick in dp per second
  //! @param deceleration how fast the flick slows down in dp per second per second (zero or more)
  //! @return the acceleration in dp per second per second, zero for a flick that does not move
  [[nodiscard]] inline DpPoint2F CalcDeceleration(const DpPoint2F velocityDpf, const DpValueF deceleration) noexcept
  {
    const float speed = velocityDpf.Length().Value;
    if (speed <= 0.0f)
    {
      return {};
    }
    const float scale = -deceleration.Value / speed;
    return DpPoint2F::Create(velocityDpf.X.Value * scale, velocityDpf.Y.Value * scale);
  }
}

#endif
