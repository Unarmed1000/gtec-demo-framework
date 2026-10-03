#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_WIN32_GPUSTATSADAPTERWIN32_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_WIN32_GPUSTATSADAPTERWIN32_HPP
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
#include <FslDemoService/SystemStats/Impl/Adapter/IGpuStatsAdapter.hpp>
#include <memory>

namespace Fsl
{
  class GpuCounterQueryWin32;

  //! The GPU usage of the application according to the GPU performance counters of Windows (Windows 10 1709 and a WDDM 2 driver).
  //! The counters are read on a thread of the adapter, as setting them up takes long enough to be seen as a stutter.
  class GpuStatsAdapterWin32 final : public IGpuStatsAdapter
  {
    std::unique_ptr<GpuCounterQueryWin32> m_query;
    mutable bool m_hasLoggedState{false};

  public:
    GpuStatsAdapterWin32();
    ~GpuStatsAdapterWin32() final;

    bool TryGetApplicationGpuUsage(GpuUsageRecord& rUsageRecord) const final;
    bool TryGetApplicationGpuMemoryUsage(GpuMemoryUsageRecord& rUsageRecord) const final;

  private:
    void LogStateOnce() const noexcept;
  };
}

#endif

#endif
