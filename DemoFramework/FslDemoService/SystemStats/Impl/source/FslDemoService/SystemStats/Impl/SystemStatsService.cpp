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


#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoService/CpuStats/Impl/Adapter/ICpuStatsAdapter.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/IGpuStatsAdapter.hpp>
#include <FslDemoService/SystemStats/Impl/SystemStatsService.hpp>
#include <exception>
#include <stdexcept>
#include <utility>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! How long a value of the graphics API is used and how long the service says it wants one after it was asked for
      constexpr TimeSpan ApiGpuMemoryUsageLifetime = TimeSpan::FromSeconds(3);
    }
  }


  SystemStatsService::SystemStatsService(const ServiceProvider& serviceProvider,
                                         const std::function<std::unique_ptr<ICpuStatsAdapter>()>& fnAllocateCpuAdapter,
                                         std::function<std::unique_ptr<IGpuStatsAdapter>()> fnAllocateGpuAdapter)
    : ThreadLocalService(serviceProvider)
    , m_fnAllocateGpuAdapter(std::move(fnAllocateGpuAdapter))
  {
    if (!fnAllocateCpuAdapter)
    {
      throw std::invalid_argument("allocate cpu adapter function must be valid");
    }
    try
    {
      m_cpuAdapter = fnAllocateCpuAdapter();
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_WARNING("Failed to allocate the CPU stats adapter {}", ex.what());
      m_cpuAdapter.reset();
    }
    // Cache the CPU count
    m_cpuCount = m_cpuAdapter ? m_cpuAdapter->GetCpuCount() : 0u;
  }


  SystemStatsService::~SystemStatsService() = default;


  void SystemStatsService::Update()
  {
    if (m_cpuAdapter)
    {
      m_cpuAdapter->Process();
    }
  }


  uint32_t SystemStatsService::GetCpuCount() const
  {
    return m_cpuCount;
  }


  bool SystemStatsService::TryGetCpuUsage(float& rUsagePercentage, const uint32_t cpuIndex) const
  {
    CpuUsageRecord record;
    const bool hasUsage = TryGetCpuUsage(record, cpuIndex);
    rUsagePercentage = hasUsage ? record.UsagePercentage : 0.0f;
    return hasUsage;
  }


  bool SystemStatsService::TryGetCpuUsage(CpuUsageRecord& rUsageRecord, const uint32_t cpuIndex) const
  {
    rUsageRecord = {};
    if (!m_cpuAdapter)
    {
      FSLLOG3_DEBUG_VERBOSE6("not available");
      return false;
    }
    if (cpuIndex >= m_cpuCount)
    {
      FSLLOG3_DEBUG_INFO("cpuIndex out of bounds");
      return false;
    }
    return m_cpuAdapter->TryGetCpuUsage(rUsageRecord, cpuIndex);
  }


  bool SystemStatsService::TryGetApplicationCpuUsage(float& rUsagePercentage) const
  {
    CpuUsageRecord record;
    const bool hasUsage = TryGetApplicationCpuUsage(record);
    rUsagePercentage = hasUsage ? record.UsagePercentage : 0.0f;
    return hasUsage;
  }


  bool SystemStatsService::TryGetApplicationCpuUsage(CpuUsageRecord& rUsageRecord) const
  {
    rUsageRecord = {};
    if (!m_cpuAdapter)
    {
      FSLLOG3_DEBUG_VERBOSE6("not available");
      return false;
    }
    return m_cpuAdapter->TryGetApplicationCpuUsage(rUsageRecord);
  }


  bool SystemStatsService::TryGetApplicationRamUsage(uint64_t& rRamUsage) const
  {
    rRamUsage = 0u;
    if (!m_cpuAdapter)
    {
      FSLLOG3_DEBUG_VERBOSE6("not available");
      return false;
    }
    return m_cpuAdapter->TryGetApplicationRamUsage(rRamUsage);
  }


  bool SystemStatsService::TryGetApplicationGpuUsage(GpuUsageRecord& rUsageRecord) const
  {
    rUsageRecord = {};
    const IGpuStatsAdapter* const pAdapter = TryGetGpuAdapter();
    if (pAdapter == nullptr || !pAdapter->TryGetApplicationGpuUsage(rUsageRecord))
    {
      rUsageRecord = {};
      return false;
    }
    return true;
  }


  bool SystemStatsService::TryGetApplicationGpuMemoryUsage(GpuMemoryUsageRecord& rUsageRecord) const
  {
    rUsageRecord = {};
    const IGpuStatsAdapter* const pAdapter = TryGetGpuAdapter();
    if (pAdapter != nullptr && pAdapter->TryGetApplicationGpuMemoryUsage(rUsageRecord))
    {
      return true;
    }

    // The operating system has no number, so use what the graphics API said (if it did so recently) and let it know it is wanted
    const TickCount currentTime = m_timer.GetTimestamp();
    m_apiGpuMemoryUsageWantedTime = currentTime;
    if (m_hasApiGpuMemoryUsage && (currentTime - m_apiGpuMemoryUsage.Timer) <= LocalConfig::ApiGpuMemoryUsageLifetime)
    {
      rUsageRecord = m_apiGpuMemoryUsage;
      return true;
    }
    rUsageRecord = {};
    return false;
  }


  bool SystemStatsService::IsApplicationGpuMemoryUsageWanted() const noexcept
  {
    return m_apiGpuMemoryUsageWantedTime.Ticks() != 0 &&
           (m_timer.GetTimestamp() - m_apiGpuMemoryUsageWantedTime) <= LocalConfig::ApiGpuMemoryUsageLifetime;
  }


  void SystemStatsService::SetApplicationGpuMemoryUsage(const uint64_t dedicatedBytes, const uint64_t sharedBytes) noexcept
  {
    m_apiGpuMemoryUsage = GpuMemoryUsageRecord(m_timer.GetTimestamp(), dedicatedBytes, sharedBytes);
    m_hasApiGpuMemoryUsage = true;
  }


  const IGpuStatsAdapter* SystemStatsService::TryGetGpuAdapter() const noexcept
  {
    if (!m_gpuAdapterAllocated)
    {
      // Only tried once: a adapter that can not be created stays unavailable, and it never takes the CPU stats with it
      m_gpuAdapterAllocated = true;
      if (m_fnAllocateGpuAdapter)
      {
        try
        {
          m_gpuAdapter = m_fnAllocateGpuAdapter();
        }
        catch (const std::exception& ex)
        {
          FSLLOG3_WARNING("GPU stats are not available, failed to allocate the adapter: {}", ex.what());
          m_gpuAdapter.reset();
        }
      }
    }
    return m_gpuAdapter.get();
  }
}
