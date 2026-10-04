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


#include <FslDemoService/SystemStats/Impl/Adapter/SystemCpuTimesReader.hpp>
#include <array>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace Fsl::SystemCpuTimesReader
{
  namespace
  {
    constexpr std::string_view CpuLinePrefix("cpu ");
    //! user, nice, system, idle, iowait, irq, softirq, steal
    constexpr std::size_t MaxFields = 8;
    //! user, nice, system, idle: what every kernel has
    constexpr std::size_t MinFields = 4;

#if defined(_WIN32)
    //! A FILETIME counts in 100ns, which is the tick of the framework
    constexpr uint64_t ToTicks(const FILETIME& value) noexcept
    {
      return (static_cast<uint64_t>(value.dwHighDateTime) << 32u) | static_cast<uint64_t>(value.dwLowDateTime);
    }
#endif
  }


  bool TryParseProcStatCpuLine(const std::string_view line, const uint64_t ticksPerJiffy, SystemCpuTimes& rTimes) noexcept
  {
    rTimes.SystemIdleTicks = 0;
    rTimes.SystemKernelTicks = 0;
    rTimes.SystemUserTicks = 0;
    if (!line.starts_with(CpuLinePrefix) || ticksPerJiffy == 0u)
    {
      return false;
    }

    std::array<uint64_t, MaxFields> fields{};
    std::size_t fieldCount = 0;
    std::size_t index = CpuLinePrefix.size();
    while (fieldCount < MaxFields && index < line.size())
    {
      while (index < line.size() && line[index] == ' ')
      {
        ++index;
      }
      if (index >= line.size() || line[index] < '0' || line[index] > '9')
      {
        break;
      }
      uint64_t value = 0;
      while (index < line.size() && line[index] >= '0' && line[index] <= '9')
      {
        value = (value * 10u) + static_cast<uint64_t>(line[index] - '0');
        ++index;
      }
      fields[fieldCount] = value;
      ++fieldCount;
    }
    if (fieldCount < MinFields)
    {
      return false;
    }
    // user + nice, system + irq + softirq + steal, idle + iowait
    rTimes.SystemUserTicks = (fields[0] + fields[1]) * ticksPerJiffy;
    rTimes.SystemKernelTicks = (fields[2] + fields[5] + fields[6] + fields[7]) * ticksPerJiffy;
    rTimes.SystemIdleTicks = (fields[3] + fields[4]) * ticksPerJiffy;
    return true;
  }


#if defined(_WIN32)

  bool TryRead(SystemCpuTimes& rTimes) noexcept
  {
    rTimes = {};
    FILETIME idleTime{};
    FILETIME kernelTime{};
    FILETIME userTime{};
    FILETIME creationTime{};
    FILETIME exitTime{};
    FILETIME processKernelTime{};
    FILETIME processUserTime{};
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime) == 0 ||
        GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &processKernelTime, &processUserTime) == 0)
    {
      return false;
    }
    const uint64_t idleTicks = ToTicks(idleTime);
    const uint64_t kernelTicks = ToTicks(kernelTime);
    rTimes.SystemIdleTicks = idleTicks;
    // Windows counts the idle time as kernel time
    rTimes.SystemKernelTicks = kernelTicks >= idleTicks ? (kernelTicks - idleTicks) : 0u;
    rTimes.SystemUserTicks = ToTicks(userTime);
    rTimes.ProcessKernelTicks = ToTicks(processKernelTime);
    rTimes.ProcessUserTicks = ToTicks(processUserTime);
    return true;
  }

#elif defined(__linux__)

  bool TryRead(SystemCpuTimes& rTimes) noexcept
  {
    rTimes = {};
    const long jiffiesPerSecond = sysconf(_SC_CLK_TCK);
    if (jiffiesPerSecond <= 0 || jiffiesPerSecond > 10000000)
    {
      return false;
    }

    std::array<char, 512> buffer{};
    std::size_t length = 0;
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
      const int fd = open("/proc/stat", O_RDONLY | O_CLOEXEC);
      if (fd < 0)
      {
        return false;
      }
      const ssize_t count = read(fd, buffer.data(), buffer.size());
      close(fd);
      if (count <= 0)
      {
        return false;
      }
      length = static_cast<std::size_t>(count);
    }
    const std::string_view content(buffer.data(), length);
    const std::string_view firstLine = content.substr(0, content.find('\n'));
    if (!TryParseProcStatCpuLine(firstLine, 10000000u / static_cast<uint64_t>(jiffiesPerSecond), rTimes))
    {
      rTimes = {};
      return false;
    }

    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
    {
      rTimes = {};
      return false;
    }
    // Seconds and microseconds to 100ns ticks
    rTimes.ProcessUserTicks = (static_cast<uint64_t>(usage.ru_utime.tv_sec) * 10000000u) + (static_cast<uint64_t>(usage.ru_utime.tv_usec) * 10u);
    rTimes.ProcessKernelTicks = (static_cast<uint64_t>(usage.ru_stime.tv_sec) * 10000000u) + (static_cast<uint64_t>(usage.ru_stime.tv_usec) * 10u);
    return true;
  }

#else

  bool TryRead(SystemCpuTimes& rTimes) noexcept
  {
    rTimes = {};
    return false;
  }

#endif
}
