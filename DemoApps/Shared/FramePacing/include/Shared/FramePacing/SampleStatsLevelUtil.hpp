#ifndef SHARED_FRAMEPACING_SAMPLESTATSLEVELUTIL_HPP
#define SHARED_FRAMEPACING_SAMPLESTATSLEVELUTIL_HPP
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


#include <FslBase/Time/TimeSpan.hpp>
#include <Shared/FramePacing/SampleStatsLevel.hpp>
#include <cstdint>

namespace Fsl::SampleStatsLevelUtil
{
  //! The late frames that are a error: one frame in twenty
  inline constexpr uint32_t LateFramesErrorPercent = 5;
  //! A frame this much longer than it was to be has missed a refresh: one and a half target frame times
  inline constexpr int64_t LongFramePercent = 150;
  //! The average frame time that is a error: five percent above the target frame time, the loop does not keep its rate
  inline constexpr int64_t SlowAveragePercent = 105;
  //! The work on a frame that is a warning and that is a error, as a share of the time the frame has
  inline constexpr int64_t WorkWarningPercent = 80;
  inline constexpr int64_t WorkErrorPercent = 100;

  //! @brief The worse of two levels
  constexpr SampleStatsLevel Max(const SampleStatsLevel lhs, const SampleStatsLevel rhs) noexcept
  {
    if (lhs == SampleStatsLevel::Error || rhs == SampleStatsLevel::Error)
    {
      return SampleStatsLevel::Error;
    }
    return (lhs == SampleStatsLevel::Warning || rhs == SampleStatsLevel::Warning) ? SampleStatsLevel::Warning : SampleStatsLevel::Normal;
  }

  //! @brief The late frames among the frames that were looked at: a warning for any, a error from LateFramesErrorPercent of them.
  constexpr SampleStatsLevel RateLateFrames(const uint32_t lateFrames, const uint32_t frames) noexcept
  {
    if (lateFrames == 0u || frames == 0u)
    {
      return SampleStatsLevel::Normal;
    }
    return (static_cast<uint64_t>(lateFrames) * 100u) >= (static_cast<uint64_t>(frames) * LateFramesErrorPercent) ? SampleStatsLevel::Error
                                                                                                                  : SampleStatsLevel::Warning;
  }

  //! @brief Frame times against the time a frame is to take: a warning when the longest one missed a refresh (LongFramePercent),
  //!        a error when the average is too long (SlowAveragePercent). Normal without a target.
  constexpr SampleStatsLevel RateFrameTime(const TimeSpan average, const TimeSpan longest, const TimeSpan target) noexcept
  {
    if (target.Ticks() <= 0)
    {
      return SampleStatsLevel::Normal;
    }
    if ((average.Ticks() * 100) >= (target.Ticks() * SlowAveragePercent))
    {
      return SampleStatsLevel::Error;
    }
    return (longest.Ticks() * 100) >= (target.Ticks() * LongFramePercent) ? SampleStatsLevel::Warning : SampleStatsLevel::Normal;
  }

  //! @brief The work on a frame against the time the frame has: a warning from WorkWarningPercent of it, a error from
  //!        WorkErrorPercent. Normal without a target.
  constexpr SampleStatsLevel RateWork(const TimeSpan work, const TimeSpan target) noexcept
  {
    if (target.Ticks() <= 0)
    {
      return SampleStatsLevel::Normal;
    }
    if ((work.Ticks() * 100) >= (target.Ticks() * WorkErrorPercent))
    {
      return SampleStatsLevel::Error;
    }
    return (work.Ticks() * 100) >= (target.Ticks() * WorkWarningPercent) ? SampleStatsLevel::Warning : SampleStatsLevel::Normal;
  }
}

#endif
