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

#include <FslBase/Exceptions.hpp>
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/System/Threading/HighResolutionSleeper.hpp>

#ifdef FSLBASE_THREAD_BACKEND_NOT_SUPPORTED
// There is no way to sleep
#elif defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__) || defined(__QNXNTO__)
#include <ctime>
#else
#include <chrono>
#include <thread>
#endif

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
#if defined(FSLBASE_THREAD_BACKEND_NOT_SUPPORTED)
      constexpr TimeSpan WakeMargin{};
      constexpr std::string_view MethodName("not supported");
#elif defined(_WIN32)
      // Measured: the 99th percentile of how late a sleep of 1 to 16 ms wakes on a idle Windows 11 machine, rounded up
      constexpr TimeSpan WakeMargin(750 * TimeSpan::TicksPerMicrosecond);
      constexpr std::string_view MethodName("high resolution waitable timer");
#elif defined(__linux__)
      // Not measured on real hardware. A virtual machine says nothing: it wakes about 500 us late, the timer of its host and not the one of Linux.
      constexpr TimeSpan WakeMargin(200 * TimeSpan::TicksPerMicrosecond);
      constexpr std::string_view MethodName("clock_nanosleep");
#elif defined(__QNXNTO__)
      // Not measured. A sleep ends on a tick of the system clock, which is 1 ms by default.
      constexpr TimeSpan WakeMargin(2 * TimeSpan::TicksPerMillisecond);
      constexpr std::string_view MethodName("clock_nanosleep");
#else
      // Not measured
      constexpr TimeSpan WakeMargin(2 * TimeSpan::TicksPerMillisecond);
      constexpr std::string_view MethodName("sleep_for");
#endif
    }
  }

#ifdef FSLBASE_THREAD_BACKEND_NOT_SUPPORTED

  struct HighResolutionSleeper::Impl
  {
    Impl()
    {
      throw NotSupportedException("HighResolutionSleeper");
    }

    void SleepUntil(const TickCount /*time*/) noexcept
    {
    }
  };

#elif defined(_WIN32)

  struct HighResolutionSleeper::Impl
  {
    HighResolutionTimer Timer;
    HANDLE Handle;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Impl()
      : Handle(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS))
    {
      if (Handle == nullptr)
      {
        throw NotSupportedException("HighResolutionSleeper: could not create a high resolution waitable timer");
      }
    }

    ~Impl()
    {
      CloseHandle(Handle);
    }

    void SleepUntil(const TickCount time) noexcept
    {
      // A timer takes the time of the system clock as a absolute time, so the time is given relative to now. A tick is the 100 ns the
      // timer counts in, and a negative value is what makes it relative.
      const TimeSpan remaining = time - Timer.GetTimestamp();
      if (remaining.Ticks() > 0)
      {
        LARGE_INTEGER dueTime{};
        dueTime.QuadPart = -remaining.Ticks();
        if (SetWaitableTimer(Handle, &dueTime, 0, nullptr, nullptr, FALSE) != FALSE)
        {
          WaitForSingleObject(Handle, INFINITE);
        }
      }
    }
  };

#elif defined(__linux__) || defined(__QNXNTO__)

  struct HighResolutionSleeper::Impl
  {
    // The clock of the HighResolutionTimer, so the time is given as it is
    void SleepUntil(const TickCount time) noexcept
    {
      constexpr int64_t NanosecondsPerTick = 100;
      if (time.Ticks() > 0)
      {
        using SafeTimespec = struct timespec;
        SafeTimespec wakeTime{};
        wakeTime.tv_sec = time.Ticks() / TickCount::TicksPerSecond;
        wakeTime.tv_nsec = (time.Ticks() % TickCount::TicksPerSecond) * NanosecondsPerTick;
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeTime, nullptr);
      }
    }
  };

#else

  struct HighResolutionSleeper::Impl
  {
    HighResolutionTimer Timer;

    void SleepUntil(const TickCount time) noexcept
    {
      const TimeSpan remaining = time - Timer.GetTimestamp();
      if (remaining.Ticks() > 0)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(remaining.Ticks() / TimeSpan::TicksPerMicrosecond));
      }
    }
  };

#endif


  HighResolutionSleeper::HighResolutionSleeper()
    : m_impl(std::make_unique<Impl>())
  {
  }


  HighResolutionSleeper::~HighResolutionSleeper() = default;


  TimeSpan HighResolutionSleeper::GetWakeMargin() noexcept
  {
    return LocalConfig::WakeMargin;
  }


  std::string_view HighResolutionSleeper::GetMethodName() noexcept
  {
    return LocalConfig::MethodName;
  }


  void HighResolutionSleeper::SleepUntil(const TickCount time) noexcept
  {
    m_impl->SleepUntil(time);
  }
}
