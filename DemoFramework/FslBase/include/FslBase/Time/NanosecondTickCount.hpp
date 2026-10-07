#ifndef FSLBASE_TIME_NANOSECONDTICKCOUNT_HPP
#define FSLBASE_TIME_NANOSECONDTICKCOUNT_HPP
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



#include <FslBase/BasicTypes.hpp>
#include <FslBase/Exceptions.hpp>
#include <FslBase/NumericCast.hpp>
#include <FslBase/Time/NanosecondTimeSpan.hpp>
#include <FslBase/UncheckedNumericCast.hpp>
#include <limits>

namespace Fsl
{
  //! A point in time with a resolution of one nanosecond, where a TickCount has one of 100 nanoseconds.
  //! The difference between two of them is a NanosecondTimeSpan.
  struct NanosecondTickCount
  {
  private:
    //! The number of nanoseconds that represent the value of the current NanosecondTickCount structure.
    //! We store this internally as a unsigned uint64_t even though this represents a signed count.
    //! This is done to ensure we get properly defined overflow and underflow behavior.
    uint64_t m_nanoseconds{0};

  public:
    //! The number of nanoseconds per tick of a TickCount
    static constexpr uint16_t NanosecondsPerTick = 100;
    //! The number of nanoseconds per microsecond
    static constexpr uint16_t NanosecondsPerMicrosecond = 1000;
    //! The number of nanoseconds per millisecond
    static constexpr int32_t NanosecondsPerMillisecond = 1000000;
    //! The number of nanoseconds per second
    static constexpr int64_t NanosecondsPerSecond = int64_t{NanosecondsPerMillisecond} * 1000;
    //! The number of nanoseconds per minute
    static constexpr int64_t NanosecondsPerMinute = NanosecondsPerSecond * 60;
    //! The number of nanoseconds per hour
    static constexpr int64_t NanosecondsPerHour = NanosecondsPerMinute * 60;
    //! The number of nanoseconds per day
    static constexpr int64_t NanosecondsPerDay = NanosecondsPerHour * 24;

    static constexpr int32_t MinDays = NumericCast<int32_t>(std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerDay);
    static constexpr int32_t MaxDays = NumericCast<int32_t>(std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerDay);
    static constexpr int32_t MinHours = NumericCast<int32_t>(std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerHour);
    static constexpr int32_t MaxHours = NumericCast<int32_t>(std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerHour);
    static constexpr int64_t MinMinutes = std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerMinute;
    static constexpr int64_t MaxMinutes = std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerMinute;
    static constexpr int64_t MinSeconds = std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerSecond;
    static constexpr int64_t MaxSeconds = std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerSecond;
    static constexpr int64_t MinMilliseconds = std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerMillisecond;
    static constexpr int64_t MaxMilliseconds = std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerMillisecond;
    static constexpr int64_t MinMicroseconds = std::numeric_limits<int64_t>::min() / NanosecondTickCount::NanosecondsPerMicrosecond;
    static constexpr int64_t MaxMicroseconds = std::numeric_limits<int64_t>::max() / NanosecondTickCount::NanosecondsPerMicrosecond;

    constexpr NanosecondTickCount() noexcept = default;

    constexpr explicit NanosecondTickCount(const int64_t nanoseconds) noexcept
      : m_nanoseconds(static_cast<uint64_t>(nanoseconds))
    {
    }

    //! @brief The whole count in nanoseconds, which is the value this type holds.
    [[nodiscard]] constexpr int64_t TotalNanoseconds() const noexcept
    {
      return static_cast<int64_t>(m_nanoseconds);
    }

    [[nodiscard]] constexpr uint64_t UnsignedTotalNanoseconds() const noexcept
    {
      return static_cast<uint64_t>(m_nanoseconds);
    }

    [[nodiscard]] constexpr int32_t Days() const noexcept
    {
      return UncheckedNumericCast<int32_t>(TotalNanoseconds() / NanosecondsPerDay);
    }

    [[nodiscard]] constexpr int32_t Hours() const noexcept
    {
      return UncheckedNumericCast<int32_t>((TotalNanoseconds() / NanosecondsPerHour) % 24);
    }

    [[nodiscard]] constexpr int32_t Minutes() const noexcept
    {
      return UncheckedNumericCast<int32_t>((TotalNanoseconds() / NanosecondsPerMinute) % 60);
    }

    [[nodiscard]] constexpr int32_t Seconds() const noexcept
    {
      return UncheckedNumericCast<int32_t>((TotalNanoseconds() / NanosecondsPerSecond) % 60);
    }

    [[nodiscard]] constexpr int32_t Milliseconds() const noexcept
    {
      return UncheckedNumericCast<int32_t>((TotalNanoseconds() / NanosecondsPerMillisecond) % 1000);
    }

    [[nodiscard]] constexpr int32_t Microseconds() const noexcept
    {
      return UncheckedNumericCast<int32_t>((TotalNanoseconds() / NanosecondsPerMicrosecond) % 1000);
    }

    [[nodiscard]] constexpr int32_t Nanoseconds() const noexcept
    {
      return UncheckedNumericCast<int32_t>(TotalNanoseconds() % 1000);
    }

    [[nodiscard]] constexpr double TotalMicroseconds() const noexcept
    {
      return static_cast<double>(TotalNanoseconds()) / static_cast<double>(NanosecondsPerMicrosecond);
    }

    [[nodiscard]] constexpr double TotalMilliseconds() const noexcept
    {
      return static_cast<double>(TotalNanoseconds()) / static_cast<double>(NanosecondsPerMillisecond);
    }

    [[nodiscard]] constexpr double TotalSeconds() const noexcept
    {
      return static_cast<double>(TotalNanoseconds()) / static_cast<double>(NanosecondsPerSecond);
    }

    [[nodiscard]] constexpr double TotalMinutes() const noexcept
    {
      return static_cast<double>(TotalNanoseconds()) / static_cast<double>(NanosecondsPerMinute);
    }

    [[nodiscard]] constexpr double TotalHours() const noexcept
    {
      return static_cast<double>(TotalNanoseconds()) / static_cast<double>(NanosecondsPerHour);
    }

    [[nodiscard]] constexpr double TotalDays() const noexcept
    {
      return static_cast<double>(TotalNanoseconds()) / static_cast<double>(NanosecondsPerDay);
    }

    [[nodiscard]] constexpr int64_t TotalMicrosecondsInt64() const noexcept
    {
      return TotalNanoseconds() / NanosecondsPerMicrosecond;
    }

    constexpr NanosecondTickCount& operator+=(const NanosecondTimeSpan rhs) noexcept
    {
      m_nanoseconds += static_cast<uint64_t>(rhs.TotalNanoseconds());
      return *this;
    }

    constexpr NanosecondTickCount& operator-=(const NanosecondTimeSpan rhs) noexcept
    {
      m_nanoseconds -= static_cast<uint64_t>(rhs.TotalNanoseconds());
      return *this;
    }


    static inline constexpr NanosecondTickCount FromDays(const int32_t value)
    {
      static_assert(std::numeric_limits<int32_t>::min() < MinDays);
      static_assert(std::numeric_limits<int32_t>::max() > MaxDays);

      if (value < MinDays)
      {
        throw UnderflowException("below minimum allowed value");
      }
      if (value > MaxDays)
      {
        throw OverflowException("exceeded maximum allowed value");
      }
      return NanosecondTickCount(UncheckedNumericCast<int64_t>(value * NanosecondsPerDay));
    }

    static inline constexpr NanosecondTickCount FromHours(const int32_t value)
    {
      static_assert(std::numeric_limits<int32_t>::min() < MinHours);
      static_assert(std::numeric_limits<int32_t>::max() > MaxHours);

      if (value < MinHours)
      {
        throw UnderflowException("below minimum allowed value");
      }
      if (value > MaxHours)
      {
        throw OverflowException("exceeded maximum allowed value");
      }
      return NanosecondTickCount(UncheckedNumericCast<int64_t>(value * NanosecondsPerHour));
    }

    static inline constexpr NanosecondTickCount FromMinutes(const int64_t value)
    {
      static_assert(std::numeric_limits<int64_t>::min() < MinMinutes);
      static_assert(std::numeric_limits<int64_t>::max() > MaxMinutes);

      if (value < MinMinutes)
      {
        throw UnderflowException("Minutes can not be negative");
      }
      if (value > MaxMinutes)
      {
        throw OverflowException("Minutes exceeded maximum allowed value");
      }
      return NanosecondTickCount(UncheckedNumericCast<int64_t>(value * NanosecondsPerMinute));
    }


    static inline constexpr NanosecondTickCount FromSeconds(const int64_t value)
    {
      static_assert(std::numeric_limits<int64_t>::min() < MinSeconds);
      static_assert(std::numeric_limits<int64_t>::max() > MaxSeconds);

      if (value < MinSeconds)
      {
        throw UnderflowException("Seconds can not be negative");
      }
      if (value > MaxSeconds)
      {
        throw OverflowException("Seconds exceeded maximum allowed value");
      }
      return NanosecondTickCount(UncheckedNumericCast<int64_t>(value * NanosecondsPerSecond));
    }


    static inline constexpr NanosecondTickCount FromMilliseconds(const int64_t value)
    {
      static_assert(std::numeric_limits<int64_t>::min() < MinMilliseconds);
      static_assert(std::numeric_limits<int64_t>::max() > MaxMilliseconds);

      if (value < MinMilliseconds)
      {
        throw UnderflowException("Milliseconds can not be negative");
      }
      if (value > MaxMilliseconds)
      {
        throw OverflowException("Milliseconds exceeded maximum allowed value");
      }
      return NanosecondTickCount(UncheckedNumericCast<int64_t>(value * NanosecondsPerMillisecond));
    }

    static inline constexpr NanosecondTickCount FromMicroseconds(const int64_t value)
    {
      static_assert(std::numeric_limits<int64_t>::min() < MinMicroseconds);
      static_assert(std::numeric_limits<int64_t>::max() > MaxMicroseconds);

      if (value < MinMicroseconds)
      {
        throw UnderflowException("Microseconds can not be negative");
      }
      if (value > MaxMicroseconds)
      {
        throw OverflowException("Microseconds exceeded maximum allowed value");
      }
      return NanosecondTickCount(UncheckedNumericCast<int64_t>(value * NanosecondsPerMicrosecond));
    }

    static inline constexpr NanosecondTickCount FromNanoseconds(const int64_t value) noexcept
    {
      return NanosecondTickCount(value);
    }
  };

  // Operator ==
  constexpr inline bool operator==(const NanosecondTickCount lhs, const NanosecondTickCount rhs) noexcept
  {
    return lhs.TotalNanoseconds() == rhs.TotalNanoseconds();
  }

  // Operator !=
  constexpr inline bool operator!=(const NanosecondTickCount lhs, const NanosecondTickCount rhs) noexcept
  {
    return lhs.TotalNanoseconds() != rhs.TotalNanoseconds();
  }

  // Operator <
  constexpr inline bool operator<(const NanosecondTickCount lhs, const NanosecondTickCount rhs) noexcept
  {
    return lhs.TotalNanoseconds() < rhs.TotalNanoseconds();
  }

  // Operator <=
  constexpr inline bool operator<=(const NanosecondTickCount lhs, const NanosecondTickCount rhs) noexcept
  {
    return lhs.TotalNanoseconds() <= rhs.TotalNanoseconds();
  }

  // Operator >
  constexpr inline bool operator>(const NanosecondTickCount lhs, const NanosecondTickCount rhs) noexcept
  {
    return lhs.TotalNanoseconds() > rhs.TotalNanoseconds();
  }

  // Operator >=
  constexpr inline bool operator>=(const NanosecondTickCount lhs, const NanosecondTickCount rhs) noexcept
  {
    return lhs.TotalNanoseconds() >= rhs.TotalNanoseconds();
  }

  // Timespan support

  constexpr inline NanosecondTickCount operator+(const NanosecondTickCount lhs, const NanosecondTimeSpan rhs) noexcept
  {
    // Allow the add to wraparound the counter
    return NanosecondTickCount(static_cast<int64_t>(lhs.UnsignedTotalNanoseconds() + static_cast<uint64_t>(rhs.TotalNanoseconds())));
  }

  constexpr inline NanosecondTickCount operator-(const NanosecondTickCount lhs, const NanosecondTimeSpan rhs) noexcept
  {
    // Allow the add to wraparound the counter
    return NanosecondTickCount(static_cast<int64_t>(lhs.UnsignedTotalNanoseconds() - static_cast<uint64_t>(rhs.TotalNanoseconds())));
  }

  constexpr inline NanosecondTimeSpan operator-(const NanosecondTickCount lhs, const NanosecondTickCount rhs) noexcept
  {
    return NanosecondTimeSpan(static_cast<int64_t>(lhs.UnsignedTotalNanoseconds() - rhs.UnsignedTotalNanoseconds()));
  }
}

#endif
