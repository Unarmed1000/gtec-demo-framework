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

#include <Shared/FramePacing/SampleBoxAnimation.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace Fsl::SampleBoxAnimation
{
  namespace
  {
    namespace LocalConfig
    {
      // The values of the videos (mb-framepacing-explained, tools/frame_pacing_video): two round trips in a clip of eight seconds at
      // the normal speed and four at the fast one, a rest of a tenth of a second at each end
      constexpr double NormalPeriodSeconds = 4.0;
      constexpr double FastPeriodSeconds = 2.0;
      constexpr double SettleSeconds = 0.1;
      // The box is 2/15 of the height, the boxes of a row are two boxes apart and the box travels four of those
      constexpr int32_t BoxHeightNumerator = 2;
      constexpr int32_t BoxHeightDenominator = 15;
      constexpr int32_t TravelBoxes = 8;
    }

    //! 0 to 1 as progress goes 0 to 1: it starts and stops gently
    double EaseSineInOut(const double progress) noexcept
    {
      return (1.0 - std::cos(std::numbers::pi * progress)) / 2.0;
    }
  }


  double GetPeriodSeconds(const SampleBoxAnimationSpeed speed) noexcept
  {
    switch (speed)
    {
    case SampleBoxAnimationSpeed::Normal:
      return LocalConfig::NormalPeriodSeconds;
    case SampleBoxAnimationSpeed::Fast:
      return LocalConfig::FastPeriodSeconds;
    case SampleBoxAnimationSpeed::Off:
    default:
      return 0.0;
    }
  }


  double CalcTravelPosition(const double animationSeconds, const SampleBoxAnimationSpeed speed) noexcept
  {
    const double period = GetPeriodSeconds(speed);
    if (period <= 0.0 || !std::isfinite(animationSeconds))
    {
      return 0.0;
    }
    const double settle = LocalConfig::SettleSeconds;
    const double move = (period / 2.0) - settle;
    // Time zero is the middle of the first rest
    const double time = animationSeconds + (settle / 2.0);
    double within = time - (std::floor(time / period) * period);
    if (within < settle)
    {
      return 0.0;
    }
    within -= settle;
    if (within < move)
    {
      return EaseSineInOut(within / move);
    }
    within -= move;
    if (within < settle)
    {
      return 1.0;
    }
    return 1.0 - EaseSineInOut(std::min((within - settle) / move, 1.0));
  }


  PxRectangle CalcBoxRectangle(const PxSize2D areaSizePx, const double animationSeconds, const SampleBoxAnimationSpeed speed) noexcept
  {
    const int32_t width = areaSizePx.RawWidth();
    const int32_t height = areaSizePx.RawHeight();
    if (width <= 0 || height <= 0)
    {
      return {};
    }
    const int32_t boxSize = std::min(std::max((height * LocalConfig::BoxHeightNumerator) / LocalConfig::BoxHeightDenominator, 1), width);
    // The path is eight boxes long where the area has room for it
    const int32_t travel = std::max(std::min(boxSize * LocalConfig::TravelBoxes, width - boxSize), 0);
    const int32_t pathLeft = (width - boxSize - travel) / 2;
    const auto boxX = pathLeft + static_cast<int32_t>(std::lround(CalcTravelPosition(animationSeconds, speed) * static_cast<double>(travel)));
    return PxRectangle::Create(boxX, (height - boxSize) / 2, boxSize, boxSize);
  }
}
