#ifndef SHARED_FRAMEPACING_SAMPLEBOXANIMATION_HPP
#define SHARED_FRAMEPACING_SAMPLEBOXANIMATION_HPP
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

#include <FslBase/Math/Pixel/PxRectangle.hpp>
#include <FslBase/Math/Pixel/PxSize2D.hpp>

namespace Fsl
{
  //! How fast the box of the box animation moves
  enum class SampleBoxAnimationSpeed
  {
    //! The animation is not shown
    Off,
    //! A round trip takes four seconds
    Normal,
    //! A round trip takes two seconds
    Fast
  };

  //! The box animation of the mb-framepacing-explained videos: a box that moves side to side along a eased path (sine in and out), with
  //! a short rest at both ends. It is the motion those videos use to show what a animation error looks like, so a sample that draws it
  //! with the time its frames are animated for can be compared with them by eye. Only the box is drawn, on top of what the sample
  //! shows.
  //!
  //! The position is a function of the animation time and nothing else, as it is in the videos: the box is where a perfectly timed
  //! frame would have it for that time.
  namespace SampleBoxAnimation
  {
    //! The time of a round trip: there, a rest, back, a rest (zero for Off)
    [[nodiscard]] double GetPeriodSeconds(const SampleBoxAnimationSpeed speed) noexcept;

    //! @brief How far the box is along its path at the given animation time.
    //! @return 0 (at the left end) to 1 (at the right end). Time zero is the middle of the rest at the left end.
    [[nodiscard]] double CalcTravelPosition(const double animationSeconds, const SampleBoxAnimationSpeed speed) noexcept;

    //! @brief Where the box is drawn in a area that starts at the top left of the window. The box is 2/15 of the height of the area,
    //!        its path eight boxes long (less if the area is too narrow) and centered in the area.
    //! @return a empty rectangle if the area is empty
    [[nodiscard]] PxRectangle CalcBoxRectangle(const PxSize2D areaSizePx, const double animationSeconds,
                                               const SampleBoxAnimationSpeed speed) noexcept;
  }
}

#endif
