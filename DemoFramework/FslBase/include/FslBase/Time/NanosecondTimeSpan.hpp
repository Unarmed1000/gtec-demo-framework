#ifndef FSLBASE_TIME_NANOSECONDTIMESPAN_HPP
#define FSLBASE_TIME_NANOSECONDTIMESPAN_HPP
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
#include <FslBase/NumericCast.hpp>
#include <FslBase/UncheckedNumericCast.hpp>
#include <cassert>
#include <limits>

namespace Fsl
{
  //! A time interval with a resolution of one nanosecond, where a TimeSpan has one of 100 nanoseconds.
  //! It is for values a tick of a TimeSpan is too coarse for, like the refresh period of a display: at 240 Hz one tick of 100
  //! nanoseconds is 24 parts per million of the period. The range is about 292 years in both directions.
  struct NanosecondTimeSpan
  {
  private:
    //! The number of nanoseconds that represent the value of the current NanosecondTimeSpan structure.
    //! The value can be negative or positive to represent a negative or positive time interval
    int64_t m_nanoseconds{0};

  public:
    //! The number of nanoseconds per tick of a TimeSpan
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


    static constexpr int32_t MinDays = NumericCast<int32_t>(std::numeric_limits<int64_t>::min() / NanosecondsPerDay);
    static constexpr int32_t MaxDays = NumericCast<int32_t>(std::numeric_limits<int64_t>::max() / NanosecondsPerDay);
    static constexpr int32_t MinHours = NumericCast<int32_t>(std::numeric_limits<int64_t>::min() / NanosecondsPerHour);
    static constexpr int32_t MaxHours = NumericCast<int32_t>(std::numeric_limits<int64_t>::max() / NanosecondsPerHour);
    static constexpr int64_t MinMinutes = std::numeric_limits<int64_t>::min() / NanosecondsPerMinute;
    static constexpr int64_t MaxMinutes = std::numeric_limits<int64_t>::max() / NanosecondsPerMinute;
    static constexpr int64_t MinSeconds = std::numeric_limits<int64_t>::min() / NanosecondsPerSecond;
    static constexpr int64_t MaxSeconds = std::numeric_limits<int64_t>::max() / NanosecondsPerSecond;
    static constexpr int64_t MinMilliseconds = std::numeric_limits<int64_t>::min() / NanosecondsPerMillisecond;
    static constexpr int64_t MaxMilliseconds = std::numeric_limits<int64_t>::max() / NanosecondsPerMillisecond;
    static constexpr int64_t MinMicroseconds = std::numeric_limits<int64_t>::min() / NanosecondsPerMicrosecond;
    static constexpr int64_t MaxMicroseconds = std::numeric_limits<int64_t>::max() / NanosecondsPerMicrosecond;

    constexpr NanosecondTimeSpan() noexcept = default;

    constexpr explicit NanosecondTimeSpan(const int64_t nanoseconds) noexcept
      : m_nanoseconds(nanoseconds)
    {
    }

    constexpr explicit NanosecondTimeSpan(const int32_t hours, const int32_t minutes, const int32_t seconds) noexcept
      : NanosecondTimeSpan((hours * NanosecondsPerHour) + (minutes * NanosecondsPerMinute) + (seconds * NanosecondsPerSecond))
    {
    }

    constexpr explicit NanosecondTimeSpan(const int32_t days, const int32_t hours, const int32_t minutes, const int32_t seconds) noexcept
      : NanosecondTimeSpan((days * NanosecondsPerDay) + (hours * NanosecondsPerHour) + (minutes * NanosecondsPerMinute) +
                           (seconds * NanosecondsPerSecond))
    {
    }

    constexpr explicit NanosecondTimeSpan(const int32_t days, const int32_t hours, const int32_t minutes, const int32_t seconds,
                                          const int32_t milliseconds) noexcept
      : NanosecondTimeSpan((days * NanosecondsPerDay) + (hours * NanosecondsPerHour) + (minutes * NanosecondsPerMinute) +
                           (seconds * NanosecondsPerSecond) + (static_cast<int64_t>(milliseconds) * NanosecondsPerMillisecond))
    {
    }


    [[nodiscard]] constexpr float DeltaTime() const
    {
      return static_cast<float>(static_cast<double>(m_nanoseconds) / 1000000000.0);
    }

    //! @brief The whole time interval in nanoseconds, which is the value this type holds.
    [[nodiscard]] constexpr int64_t TotalNanoseconds() const noexcept
    {
      return m_nanoseconds;
    }

    [[nodiscard]] constexpr int32_t Days() const noexcept
    {
      return UncheckedNumericCast<int32_t>(m_nanoseconds / NanosecondsPerDay);
    }

    [[nodiscard]] constexpr int32_t Hours() const noexcept
    {
      return UncheckedNumericCast<int32_t>((m_nanoseconds / NanosecondsPerHour) % 24);
    }

    [[nodiscard]] constexpr int32_t Minutes() const noexcept
    {
      return UncheckedNumericCast<int32_t>((m_nanoseconds / NanosecondsPerMinute) % 60);
    }

    [[nodiscard]] constexpr int32_t Seconds() const noexcept
    {
      return UncheckedNumericCast<int32_t>((m_nanoseconds / NanosecondsPerSecond) % 60);
    }

    [[nodiscard]] constexpr int32_t Milliseconds() const noexcept
    {
      return UncheckedNumericCast<int32_t>((m_nanoseconds / NanosecondsPerMillisecond) % 1000);
    }

    [[nodiscard]] constexpr int32_t Microseconds() const noexcept
    {
      return UncheckedNumericCast<int32_t>((m_nanoseconds / NanosecondsPerMicrosecond) % 1000);
    }

    [[nodiscard]] constexpr int32_t Nanoseconds() const noexcept
    {
      return UncheckedNumericCast<int32_t>(m_nanoseconds % 1000);
    }

    [[nodiscard]] constexpr double TotalMicroseconds() const noexcept
    {
      return static_cast<double>(m_nanoseconds) / static_cast<double>(NanosecondsPerMicrosecond);
    }

    [[nodiscard]] constexpr double TotalMilliseconds() const noexcept
    {
      return static_cast<double>(m_nanoseconds) / static_cast<double>(NanosecondsPerMillisecond);
    }

    [[nodiscard]] constexpr double TotalSeconds() const noexcept
    {
      return static_cast<double>(m_nanoseconds) / static_cast<double>(NanosecondsPerSecond);
    }

    [[nodiscard]] constexpr double TotalMinutes() const noexcept
    {
      return static_cast<double>(m_nanoseconds) / static_cast<double>(NanosecondsPerMinute);
    }

    [[nodiscard]] constexpr double TotalHours() const noexcept
    {
      return static_cast<double>(m_nanoseconds) / static_cast<double>(NanosecondsPerHour);
    }

    [[nodiscard]] constexpr double TotalDays() const noexcept
    {
      return static_cast<double>(m_nanoseconds) / static_cast<double>(NanosecondsPerDay);
    }


    constexpr NanosecondTimeSpan operator-() const noexcept
    {
      return NanosecondTimeSpan(-m_nanoseconds);
    }

    constexpr NanosecondTimeSpan& operator+=(const NanosecondTimeSpan arg) noexcept
    {
      m_nanoseconds += arg.m_nanoseconds;
      return *this;
    }

    constexpr NanosecondTimeSpan& operator-=(const NanosecondTimeSpan arg) noexcept
    {
      m_nanoseconds -= arg.m_nanoseconds;
      return *this;
    }

    constexpr NanosecondTimeSpan& operator*=(const int32_t arg) noexcept
    {
      m_nanoseconds *= arg;
      return *this;
    }

    constexpr NanosecondTimeSpan& operator/=(const int32_t arg) noexcept
    {
      m_nanoseconds /= arg;
      return *this;
    }

    static inline constexpr NanosecondTimeSpan FromDays(const int32_t value)
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
      return NanosecondTimeSpan(value * NanosecondsPerDay);
    }

    static inline constexpr NanosecondTimeSpan FromHours(const int32_t value)
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
      return NanosecondTimeSpan(value * NanosecondsPerHour);
    }

    static inline constexpr NanosecondTimeSpan FromMinutes(const int64_t value)
    {
      static_assert(std::numeric_limits<int64_t>::min() < MinMinutes);
      static_assert(std::numeric_limits<int64_t>::max() > MaxMinutes);

      if (value < MinMinutes)
      {
        throw UnderflowException("below minimum allowed value");
      }
      if (value > MaxMinutes)
      {
        throw OverflowException("exceeded maximum allowed value");
      }
      return NanosecondTimeSpan(UncheckedNumericCast<int64_t>(value * NanosecondsPerMinute));
    }

    static inline constexpr NanosecondTimeSpan FromSeconds(const int32_t value)
    {
      return FromSeconds(UncheckedNumericCast<int64_t>(value));
    }

    static inline constexpr NanosecondTimeSpan FromSeconds(const uint32_t value)
    {
      return FromSeconds(UncheckedNumericCast<int64_t>(value));
    }

    static inline constexpr NanosecondTimeSpan FromSeconds(const int64_t value)
    {
      static_assert(std::numeric_limits<int64_t>::min() < MinSeconds);
      static_assert(std::numeric_limits<int64_t>::max() > MaxSeconds);

      if (value < MinSeconds)
      {
        throw UnderflowException("below minimum allowed value");
      }
      if (value > MaxSeconds)
      {
        throw OverflowException("exceeded maximum allowed value");
      }
      return NanosecondTimeSpan(value * NanosecondsPerSecond);
    }

    static inline constexpr NanosecondTimeSpan FromSeconds(const double value)
    {
      if (value < static_cast<double>(MinSeconds))
      {
        throw UnderflowException("below minimum allowed value");
      }
      if (value > static_cast<double>(MaxSeconds))
      {
        throw OverflowException("exceeded maximum allowed value");
      }
      const double result = value * static_cast<double>(NanosecondsPerSecond);
      return NanosecondTimeSpan(static_cast<int64_t>(result));
    }


    static inline constexpr NanosecondTimeSpan FromMilliseconds(const int64_t value)
    {
      static_assert(std::numeric_limits<int64_t>::min() < MinMilliseconds);
      static_assert(std::numeric_limits<int64_t>::max() > MaxMilliseconds);

      if (value < MinMilliseconds)
      {
        throw UnderflowException("below minimum allowed value");
      }
      if (value > MaxMilliseconds)
      {
        throw OverflowException("exceeded maximum allowed value");
      }
      return NanosecondTimeSpan(UncheckedNumericCast<int64_t>(value * NanosecondsPerMillisecond));
    }

    static inline constexpr NanosecondTimeSpan FromMicroseconds(const int64_t value)
    {
      static_assert(std::numeric_limits<int64_t>::min() < MinMicroseconds);
      static_assert(std::numeric_limits<int64_t>::max() > MaxMicroseconds);

      if (value < MinMicroseconds)
      {
        throw UnderflowException("below minimum allowed value");
      }
      if (value > MaxMicroseconds)
      {
        throw OverflowException("exceeded maximum allowed value");
      }
      return NanosecondTimeSpan(UncheckedNumericCast<int64_t>(value * NanosecondsPerMicrosecond));
    }

    static inline constexpr NanosecondTimeSpan FromNanoseconds(const int64_t value) noexcept
    {
      return NanosecondTimeSpan(value);
    }
  };

  // Operator ==
  constexpr inline bool operator==(const NanosecondTimeSpan lhs, const NanosecondTimeSpan rhs) noexcept
  {
    return lhs.TotalNanoseconds() == rhs.TotalNanoseconds();
  }

  // Operator !=
  constexpr inline bool operator!=(const NanosecondTimeSpan lhs, const NanosecondTimeSpan rhs) noexcept
  {
    return lhs.TotalNanoseconds() != rhs.TotalNanoseconds();
  }

  // Operator <
  constexpr inline bool operator<(const NanosecondTimeSpan lhs, const NanosecondTimeSpan rhs) noexcept
  {
    return lhs.TotalNanoseconds() < rhs.TotalNanoseconds();
  }

  // Operator <=
  constexpr inline bool operator<=(const NanosecondTimeSpan lhs, const NanosecondTimeSpan rhs) noexcept
  {
    return lhs.TotalNanoseconds() <= rhs.TotalNanoseconds();
  }

  // Operator >
  constexpr inline bool operator>(const NanosecondTimeSpan lhs, const NanosecondTimeSpan rhs) noexcept
  {
    return lhs.TotalNanoseconds() > rhs.TotalNanoseconds();
  }

  // Operator >=
  constexpr inline bool operator>=(const NanosecondTimeSpan lhs, const NanosecondTimeSpan rhs) noexcept
  {
    return lhs.TotalNanoseconds() >= rhs.TotalNanoseconds();
  }

  constexpr inline NanosecondTimeSpan operator+(const NanosecondTimeSpan lhs, const NanosecondTimeSpan rhs) noexcept
  {
    return NanosecondTimeSpan(lhs.TotalNanoseconds() + rhs.TotalNanoseconds());
  }

  constexpr inline NanosecondTimeSpan operator-(const NanosecondTimeSpan lhs, const NanosecondTimeSpan rhs) noexcept
  {
    return NanosecondTimeSpan(lhs.TotalNanoseconds() - rhs.TotalNanoseconds());
  }

  constexpr inline NanosecondTimeSpan operator*(const NanosecondTimeSpan lhs, const int32_t rhs) noexcept
  {
    return NanosecondTimeSpan(lhs.TotalNanoseconds() * rhs);
  }

  constexpr inline NanosecondTimeSpan operator*(const int32_t lhs, const NanosecondTimeSpan rhs) noexcept
  {
    return NanosecondTimeSpan(lhs * rhs.TotalNanoseconds());
  }

  constexpr inline NanosecondTimeSpan operator/(const NanosecondTimeSpan lhs, const int32_t rhs) noexcept
  {
    return NanosecondTimeSpan(lhs.TotalNanoseconds() / rhs);
  }
}

#endif
