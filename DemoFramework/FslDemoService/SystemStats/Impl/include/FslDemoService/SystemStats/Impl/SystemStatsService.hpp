#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_SYSTEMSTATSSERVICE_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_SYSTEMSTATSSERVICE_HPP
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


#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslDemoService/SystemStats/Control/ISystemStatsServiceControl.hpp>
#include <FslDemoService/SystemStats/ISystemStatsService.hpp>
#include <FslService/Consumer/ServiceProvider.hpp>
#include <FslService/Impl/ServiceType/Local/ThreadLocalService.hpp>
#include <functional>
#include <memory>

namespace Fsl
{
  class ICpuStatsAdapter;
  class IGpuStatsAdapter;

  //! The CPU, RAM and GPU usage of the application. The CPU and RAM part is what CpuStatsService does, with the same adapters.
  class SystemStatsService final
    : public ThreadLocalService
    , public ISystemStatsService
    , public ISystemStatsServiceControl
  {
    std::unique_ptr<ICpuStatsAdapter> m_cpuAdapter;
    uint32_t m_cpuCount{0};

    std::function<std::unique_ptr<IGpuStatsAdapter>()> m_fnAllocateGpuAdapter;
    //! Created when the GPU usage is asked for the first time
    mutable std::unique_ptr<IGpuStatsAdapter> m_gpuAdapter;
    mutable bool m_gpuAdapterAllocated{false};

    HighResolutionTimer m_timer;
    //! What the graphics API said the application uses (ISystemStatsServiceControl), valid if m_hasApiGpuMemoryUsage
    GpuMemoryUsageRecord m_apiGpuMemoryUsage;
    bool m_hasApiGpuMemoryUsage{false};
    //! When the GPU memory usage was asked for and the adapter did not have it (zero = never)
    mutable TickCount m_apiGpuMemoryUsageWantedTime;

  public:
    //! @param fnAllocateCpuAdapter creates the adapter for the CPU and RAM usage (required).
    //! @param fnAllocateGpuAdapter creates the adapter for the GPU usage (can be empty: the platform has none).
    SystemStatsService(const ServiceProvider& serviceProvider, const std::function<std::unique_ptr<ICpuStatsAdapter>()>& fnAllocateCpuAdapter,
                       std::function<std::unique_ptr<IGpuStatsAdapter>()> fnAllocateGpuAdapter);
    ~SystemStatsService() final;

    void Update() final;

    // From ICpuStatsService
    [[nodiscard]] uint32_t GetCpuCount() const final;
    bool TryGetCpuUsage(float& rUsagePercentage, const uint32_t cpuIndex) const final;
    bool TryGetCpuUsage(CpuUsageRecord& rUsageRecord, const uint32_t cpuIndex) const final;
    bool TryGetApplicationCpuUsage(float& rUsagePercentage) const final;
    bool TryGetApplicationCpuUsage(CpuUsageRecord& rUsageRecord) const final;
    bool TryGetApplicationRamUsage(uint64_t& rRamUsage) const final;

    // From ISystemStatsService
    bool TryGetApplicationGpuUsage(GpuUsageRecord& rUsageRecord) const final;
    bool TryGetApplicationGpuMemoryUsage(GpuMemoryUsageRecord& rUsageRecord) const final;
    bool TryGetCpuTimes(SystemCpuTimes& rTimes) const final;

    // From ISystemStatsServiceControl
    [[nodiscard]] bool IsApplicationGpuMemoryUsageWanted() const noexcept final;
    void SetApplicationGpuMemoryUsage(const uint64_t dedicatedBytes, const uint64_t sharedBytes) noexcept final;

  private:
    //! @return the GPU adapter (nullptr if the platform has none or it could not be created)
    [[nodiscard]] const IGpuStatsAdapter* TryGetGpuAdapter() const noexcept;
  };
}

#endif
