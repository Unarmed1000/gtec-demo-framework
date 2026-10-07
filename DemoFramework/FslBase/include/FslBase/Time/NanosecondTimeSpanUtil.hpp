#ifndef FSLBASE_TIME_NANOSECONDTIMESPANUTIL_HPP
#define FSLBASE_TIME_NANOSECONDTIMESPANUTIL_HPP
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



#include <FslBase/NumericCast.hpp>
#include <FslBase/Time/NanosecondTimeSpan.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <limits>

namespace Fsl::NanosecondTimeSpanUtil
{
  //! @brief Convert a frequency given as a rational (numerator / denominator Hz) to the period of one cycle.
  //!        Example: 60000/1001 Hz (59.94 Hz) -> 16683333 nanoseconds.
  //! @return the period rounded to the nearest nanosecond or NanosecondTimeSpan() if the input is invalid or the result can not be
  //!         represented.
  inline constexpr NanosecondTimeSpan FromFrequencyRational(const uint64_t numerator, const uint64_t denominator) noexcept
  {
    constexpr auto NanosecondsPerSecond = static_cast<uint64_t>(NanosecondTimeSpan::NanosecondsPerSecond);
    if (numerator == 0u || denominator == 0u || denominator > (std::numeric_limits<uint64_t>::max() / NanosecondsPerSecond))
    {
      return {};
    }
    const uint64_t scaledDenominator = denominator * NanosecondsPerSecond;
    const uint64_t halfNumerator = numerator / 2u;
    if (scaledDenominator > (std::numeric_limits<uint64_t>::max() - halfNumerator))
    {
      return {};
    }
    const uint64_t nanoseconds = (scaledDenominator + halfNumerator) / numerator;
    if (nanoseconds > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
    {
      return {};
    }
    return NanosecondTimeSpan(static_cast<int64_t>(nanoseconds));
  }

  // -----------------------------------------------------------------------------------------------------------------------------------------------

  //! @brief The frequency a period is the cycle of, in Hz.
  //! @return the frequency or 0.0 if the period is not above zero.
  inline constexpr double ToFrequencyHz(const NanosecondTimeSpan period) noexcept
  {
    return period.TotalNanoseconds() > 0
             ? static_cast<double>(NanosecondTimeSpan::NanosecondsPerSecond) / static_cast<double>(period.TotalNanoseconds())
             : 0.0;
  }

  // -----------------------------------------------------------------------------------------------------------------------------------------------

  //! @brief Convert to a TimeSpan. A TimeSpan has a resolution of 100 nanoseconds, so the value is rounded to the nearest tick
  //!        (half a tick is rounded away from zero).
  inline constexpr TimeSpan ToTimeSpan(const NanosecondTimeSpan value) noexcept
  {
    constexpr int64_t NanosecondsPerTick = NanosecondTimeSpan::NanosecondsPerTick;
    const int64_t nanoseconds = value.TotalNanoseconds();
    int64_t ticks = nanoseconds / NanosecondsPerTick;
    const int64_t rest = nanoseconds % NanosecondsPerTick;
    if (rest >= (NanosecondsPerTick / 2))
    {
      ++ticks;
    }
    else if (rest <= -(NanosecondsPerTick / 2))
    {
      --ticks;
    }
    return TimeSpan(ticks);
  }

  // -----------------------------------------------------------------------------------------------------------------------------------------------

  //! @brief Convert from a TimeSpan, which is exact. A TimeSpan has a hundred times the range.
  inline constexpr NanosecondTimeSpan FromTimeSpan(const TimeSpan value)
  {
    constexpr int64_t NanosecondsPerTick = NanosecondTimeSpan::NanosecondsPerTick;
    if (value.Ticks() < (std::numeric_limits<int64_t>::min() / NanosecondsPerTick))
    {
      throw UnderflowException("nanoseconds underflow");
    }
    if (value.Ticks() > (std::numeric_limits<int64_t>::max() / NanosecondsPerTick))
    {
      throw OverflowException("nanoseconds overflow");
    }
    return NanosecondTimeSpan(value.Ticks() * NanosecondsPerTick);
  }
}

#endif
