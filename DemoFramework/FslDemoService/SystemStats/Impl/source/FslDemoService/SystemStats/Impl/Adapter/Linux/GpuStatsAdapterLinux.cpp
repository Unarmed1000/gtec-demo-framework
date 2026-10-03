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


#ifdef __linux__
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/Linux/DrmFdInfoParser.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/Linux/GpuStatsAdapterLinux.hpp>
#include <dirent.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <unistd.h>
#include <array>
#include <exception>
#include <memory>
#include <string_view>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr TimeSpan MinInterval = TimeSpan::FromSeconds(1);
      constexpr const char* const FdDir = "/proc/self/fd";
      //! A GPU device is a character device below this
      constexpr std::string_view DrmDevicePrefix("/dev/dri/");
    }

    //! @return true if the file descriptor of the process is a open GPU device
    bool IsDrmDevice(const char* const pszFdName)
    {
      std::array<char, 64> linkPath{};
      const auto res = fmt::format_to_n(linkPath.data(), linkPath.size() - 1u, "/proc/self/fd/{}", pszFdName);
      if (res.size >= linkPath.size())
      {
        return false;
      }
      linkPath[res.size] = 0;

      std::array<char, 128> target{};
      const ssize_t length = readlink(linkPath.data(), target.data(), target.size());
      return length > 0 && std::string_view(target.data(), static_cast<std::size_t>(length)).starts_with(LocalConfig::DrmDevicePrefix);
    }

    //! Read the fdinfo of a file descriptor of the process.
    //! @return the number of characters read (zero if it could not be read)
    std::size_t ReadFdInfo(const char* const pszFdName, std::array<char, 4096>& rBuffer)
    {
      std::array<char, 64> path{};
      const auto res = fmt::format_to_n(path.data(), path.size() - 1u, "/proc/self/fdinfo/{}", pszFdName);
      if (res.size >= path.size())
      {
        return 0;
      }
      path[res.size] = 0;

      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
      const int fd = open(path.data(), O_RDONLY | O_CLOEXEC);
      if (fd < 0)
      {
        return 0;
      }
      std::size_t total = 0;
      while (total < rBuffer.size())
      {
        const ssize_t count = read(fd, rBuffer.data() + total, rBuffer.size() - total);
        if (count <= 0)
        {
          break;
        }
        total += static_cast<std::size_t>(count);
      }
      close(fd);
      return total;
    }
  }


  GpuStatsAdapterLinux::GpuStatsAdapterLinux() = default;


  bool GpuStatsAdapterLinux::TryGetApplicationGpuUsage(GpuUsageRecord& rUsageRecord) const
  {
    SampleIfNeeded();
    float usagePercentage = 0.0f;
    if (m_hasSample && m_aggregator.TryGetUsagePercentage(usagePercentage))
    {
      rUsageRecord = GpuUsageRecord(m_lastSampleTime, usagePercentage);
      return true;
    }
    rUsageRecord = {};
    return false;
  }


  bool GpuStatsAdapterLinux::TryGetApplicationGpuMemoryUsage(GpuMemoryUsageRecord& rUsageRecord) const
  {
    SampleIfNeeded();
    uint64_t memoryBytes = 0;
    if (m_hasSample && m_aggregator.TryGetMemoryBytes(memoryBytes))
    {
      rUsageRecord = GpuMemoryUsageRecord(m_lastSampleTime, memoryBytes, 0u);
      return true;
    }
    rUsageRecord = {};
    return false;
  }


  void GpuStatsAdapterLinux::SampleIfNeeded() const noexcept
  {
    if (!m_enabled)
    {
      return;
    }
    const TickCount currentTime = m_timer.GetTimestamp();
    if (m_hasSample && (currentTime - m_lastSampleTime) < LocalConfig::MinInterval)
    {
      return;
    }
    try
    {
      Sample(currentTime);
    }
    catch (const std::exception&)
    {
      // Nothing that is expected to get better, so the GPU stats are not available from now on
      m_enabled = false;
      m_hasSample = false;
    }
  }


  void GpuStatsAdapterLinux::Sample(const TickCount currentTime) const
  {
    const std::unique_ptr<DIR, int (*)(DIR*)> dir(opendir(LocalConfig::FdDir), closedir);
    if (!dir)
    {
      // No access to the file descriptors of the process, that will not change
      m_enabled = false;
      FSLLOG3_VERBOSE("GPU stats: not available, '{}' can not be read", LocalConfig::FdDir);
      return;
    }

    std::array<char, 4096> buffer{};
    m_aggregator.BeginSample();
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    for (const dirent* pEntry = readdir(dir.get()); pEntry != nullptr; pEntry = readdir(dir.get()))
    {
      const char* const pszName = pEntry->d_name;
      if (pszName[0] < '0' || pszName[0] > '9' || !IsDrmDevice(pszName))
      {
        continue;
      }
      const std::size_t length = ReadFdInfo(pszName, buffer);
      if (length > 0u)
      {
        m_aggregator.AddClient(DrmFdInfoParser::Parse(std::string_view(buffer.data(), length)));
      }
    }
    m_aggregator.EndSample(currentTime);
    m_lastSampleTime = currentTime;
    m_hasSample = true;
  }
}

#endif
