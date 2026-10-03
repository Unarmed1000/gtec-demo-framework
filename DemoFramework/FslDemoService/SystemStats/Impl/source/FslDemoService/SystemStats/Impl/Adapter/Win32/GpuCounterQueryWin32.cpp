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


#if defined(_WIN32) && defined(FSL_PLATFORM_WINDOWS)
#include "GpuCounterQueryWin32.hpp"
#include <FslDemoService/SystemStats/Impl/Adapter/Win32/GpuProcessUsageAggregator.hpp>
#include <fmt/format.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <iterator>
#include <string_view>
#include <vector>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr std::chrono::milliseconds CollectInterval(1000);
      //! A measurement that is older than this is not returned (the thread stopped measuring, or the machine was suspended)
      constexpr TimeSpan MaxAge = TimeSpan::FromSeconds(3);
    }

    //! Closes the query when the thread is done with it
    class ScopedQuery final
    {
      PDH_HQUERY m_hQuery{nullptr};

    public:
      ScopedQuery(const ScopedQuery&) = delete;
      ScopedQuery& operator=(const ScopedQuery&) = delete;

      ScopedQuery() noexcept = default;
      ~ScopedQuery()
      {
        if (m_hQuery != nullptr)
        {
          PdhCloseQuery(m_hQuery);
        }
      }

      PDH_STATUS Open() noexcept
      {
        return PdhOpenQuery(nullptr, 0, &m_hQuery);
      }

      [[nodiscard]] PDH_HQUERY Get() const noexcept
      {
        return m_hQuery;
      }
    };

    //! Add a counter of the instances of this process. The wildcard is kept by the query, so instances that appear later are read as well.
    PDH_STATUS AddProcessCounter(const PDH_HQUERY hQuery, const char* const pszObject, const char* const pszCounter, const uint32_t processId,
                                 PDH_HCOUNTER& rCounter)
    {
      fmt::memory_buffer path;
      fmt::format_to(std::back_inserter(path), "\\{}(pid_{}_*)\\{}", pszObject, processId, pszCounter);
      path.push_back(0);
      rCounter = nullptr;
      // The English names work no matter the language of Windows
      return PdhAddEnglishCounterA(hQuery, path.data(), 0, &rCounter);
    }

    //! Read every instance of a counter.
    //! @return the instances, a empty span if there are none (yet)
    template <typename TFunc>
    void ForEachInstance(const PDH_HCOUNTER hCounter, const DWORD format, std::vector<uint8_t>& rBuffer, const TFunc& fnInstance)
    {
      if (hCounter == nullptr)
      {
        return;
      }
      // The size is asked for first, as the documentation requires
      DWORD bufferSize = 0;
      DWORD itemCount = 0;
      PDH_STATUS status = PdhGetFormattedCounterArrayA(hCounter, format, &bufferSize, &itemCount, nullptr);
      if (static_cast<DWORD>(status) != PDH_MORE_DATA || bufferSize == 0u)
      {
        return;
      }
      if (rBuffer.size() < bufferSize)
      {
        rBuffer.resize(bufferSize);
      }
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      auto* const pItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_A*>(rBuffer.data());
      status = PdhGetFormattedCounterArrayA(hCounter, format, &bufferSize, &itemCount, pItems);
      if (status != ERROR_SUCCESS)
      {
        // The instances changed between the two calls, or the first read of a new instance: there is a new chance in a second
        return;
      }
      for (DWORD i = 0; i < itemCount; ++i)
      {
        const PDH_FMT_COUNTERVALUE_ITEM_A& item = pItems[i];
        if (item.szName != nullptr && (item.FmtValue.CStatus == PDH_CSTATUS_VALID_DATA || item.FmtValue.CStatus == PDH_CSTATUS_NEW_DATA))
        {
          fnInstance(std::string_view(item.szName), item.FmtValue);
        }
      }
    }
  }


  GpuCounterQueryWin32::GpuCounterQueryWin32()
    : m_processId(GetCurrentProcessId())
    , m_thread([this]() { Run(); })
  {
  }


  GpuCounterQueryWin32::~GpuCounterQueryWin32()
  {
    {
      const std::lock_guard<std::mutex> lock(m_mutex);
      m_stop = true;
    }
    m_wake.notify_all();
    if (m_thread.joinable())
    {
      m_thread.join();
    }
  }


  GpuCounterQueryState GpuCounterQueryWin32::GetState() const noexcept
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot.State;
  }


  bool GpuCounterQueryWin32::TryGetUsage(GpuUsageRecord& rUsageRecord) const noexcept
  {
    const TickCount currentTime = m_timer.GetTimestamp();
    {
      const std::lock_guard<std::mutex> lock(m_mutex);
      if (m_snapshot.HasUsage && (currentTime - m_snapshot.Usage.Timer) <= LocalConfig::MaxAge)
      {
        rUsageRecord = m_snapshot.Usage;
        return true;
      }
    }
    rUsageRecord = {};
    return false;
  }


  bool GpuCounterQueryWin32::TryGetMemoryUsage(GpuMemoryUsageRecord& rUsageRecord) const noexcept
  {
    const TickCount currentTime = m_timer.GetTimestamp();
    {
      const std::lock_guard<std::mutex> lock(m_mutex);
      if (m_snapshot.HasMemoryUsage && (currentTime - m_snapshot.MemoryUsage.Timer) <= LocalConfig::MaxAge)
      {
        rUsageRecord = m_snapshot.MemoryUsage;
        return true;
      }
    }
    rUsageRecord = {};
    return false;
  }


  void GpuCounterQueryWin32::Run() noexcept
  {
    try
    {
      RunQuery();
    }
    catch (...)
    {
      // Nothing is logged from this thread, the state tells the owner
      PublishNotSupported(static_cast<uint32_t>(PDH_INVALID_DATA));
    }
  }


  void GpuCounterQueryWin32::RunQuery()
  {
    const TickCount setupStartTime = m_timer.GetTimestamp();

    ScopedQuery query;
    PDH_STATUS status = query.Open();
    if (status != ERROR_SUCCESS)
    {
      PublishNotSupported(static_cast<uint32_t>(status));
      return;
    }

    PDH_HCOUNTER hUtilization = nullptr;
    PDH_HCOUNTER hDedicated = nullptr;
    PDH_HCOUNTER hShared = nullptr;
    const PDH_STATUS utilizationStatus = AddProcessCounter(query.Get(), "GPU Engine", "Utilization Percentage", m_processId, hUtilization);
    const PDH_STATUS dedicatedStatus = AddProcessCounter(query.Get(), "GPU Process Memory", "Dedicated Usage", m_processId, hDedicated);
    const PDH_STATUS sharedStatus = AddProcessCounter(query.Get(), "GPU Process Memory", "Shared Usage", m_processId, hShared);
    if (utilizationStatus != ERROR_SUCCESS && dedicatedStatus != ERROR_SUCCESS && sharedStatus != ERROR_SUCCESS)
    {
      // A Windows or a driver without the GPU counters
      PublishNotSupported(static_cast<uint32_t>(utilizationStatus));
      return;
    }

    {
      const std::lock_guard<std::mutex> lock(m_mutex);
      m_snapshot.State.Setup = GpuCounterQuerySetup::Ready;
      m_snapshot.State.SetupTime = m_timer.GetTimestamp() - setupStartTime;
    }

    GpuProcessUsageAggregator aggregator(m_processId);
    std::vector<uint8_t> buffer;
    do
    {
      const TickCount collectStartTime = m_timer.GetTimestamp();
      // PDH_NO_DATA is what a process without a graphics device gets, so it is not a failure
      status = PdhCollectQueryData(query.Get());
      aggregator.Clear();
      if (status == ERROR_SUCCESS)
      {
        ForEachInstance(hUtilization, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, buffer,
                        [&aggregator](const std::string_view name, const PDH_FMT_COUNTERVALUE& value)
                        { aggregator.AddEngineUtilization(name, value.doubleValue); });
        ForEachInstance(hDedicated, PDH_FMT_LARGE, buffer, [&aggregator](const std::string_view name, const PDH_FMT_COUNTERVALUE& value)
                        { aggregator.AddDedicatedUsage(name, static_cast<uint64_t>(std::max(value.largeValue, LONGLONG{0}))); });
        ForEachInstance(hShared, PDH_FMT_LARGE, buffer, [&aggregator](const std::string_view name, const PDH_FMT_COUNTERVALUE& value)
                        { aggregator.AddSharedUsage(name, static_cast<uint64_t>(std::max(value.largeValue, LONGLONG{0}))); });
      }
      const TickCount currentTime = m_timer.GetTimestamp();

      float usagePercentage = 0.0f;
      const bool hasUsage = aggregator.TryGetUsagePercentage(usagePercentage);
      uint64_t dedicatedBytes = 0;
      uint64_t sharedBytes = 0;
      const bool hasMemoryUsage = aggregator.TryGetMemoryUsage(dedicatedBytes, sharedBytes);
      {
        const std::lock_guard<std::mutex> lock(m_mutex);
        // A collect without a result keeps the previous one, which is dropped by its age
        if (hasUsage)
        {
          m_snapshot.HasUsage = true;
          m_snapshot.Usage = GpuUsageRecord(currentTime, usagePercentage);
        }
        if (hasMemoryUsage)
        {
          m_snapshot.HasMemoryUsage = true;
          m_snapshot.MemoryUsage = GpuMemoryUsageRecord(currentTime, dedicatedBytes, sharedBytes);
        }
        m_snapshot.State.MaxCollectTime = std::max(m_snapshot.State.MaxCollectTime, currentTime - collectStartTime);
      }
    } while (WaitForNextCollect());
  }


  void GpuCounterQueryWin32::PublishNotSupported(const uint32_t status) noexcept
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.State.Setup = GpuCounterQuerySetup::NotSupported;
    m_snapshot.State.SetupStatus = status;
    m_snapshot.HasUsage = false;
    m_snapshot.HasMemoryUsage = false;
  }


  bool GpuCounterQueryWin32::WaitForNextCollect()
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_wake.wait_for(lock, LocalConfig::CollectInterval, [this]() { return m_stop; });
    return !m_stop;
  }
}

#endif
