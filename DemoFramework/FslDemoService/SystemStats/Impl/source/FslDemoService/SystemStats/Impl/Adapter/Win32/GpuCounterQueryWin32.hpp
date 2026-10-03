#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_WIN32_GPUCOUNTERQUERYWIN32_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_WIN32_GPUCOUNTERQUERYWIN32_HPP
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
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoService/SystemStats/GpuMemoryUsageRecord.hpp>
#include <FslDemoService/SystemStats/GpuUsageRecord.hpp>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace Fsl
{
  enum class GpuCounterQuerySetup
  {
    //! The thread is setting the counters up
    Pending,
    //! The counters are being read
    Ready,
    //! The counters do not exist or could not be set up, nothing will be measured
    NotSupported
  };

  struct GpuCounterQueryState
  {
    GpuCounterQuerySetup Setup{GpuCounterQuerySetup::Pending};
    //! How long the setup took
    TimeSpan SetupTime;
    //! The status the setup failed with (zero if it did not)
    uint32_t SetupStatus{0};
    //! The longest it took to read the counters
    TimeSpan MaxCollectTime;
  };


  //! Reads the GPU performance counters of this process ("GPU Engine" and "GPU Process Memory") once a second on a thread of its own.
  //!
  //! Everything that touches the counters is done by the thread: adding the first counter of the GPU object was measured to take 180 ms,
  //! and reading them enumerates every process that uses the GPU. The caller only copies the last result.
  class GpuCounterQueryWin32 final
  {
    struct Snapshot
    {
      GpuCounterQueryState State;
      bool HasUsage{false};
      GpuUsageRecord Usage;
      bool HasMemoryUsage{false};
      GpuMemoryUsageRecord MemoryUsage;
    };

    HighResolutionTimer m_timer;
    uint32_t m_processId{0};
    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    //! Guarded by m_mutex
    Snapshot m_snapshot;
    //! Guarded by m_mutex
    bool m_stop{false};
    //! The last member, so everything it uses exists when it starts
    std::thread m_thread;

  public:
    GpuCounterQueryWin32(const GpuCounterQueryWin32&) = delete;
    GpuCounterQueryWin32& operator=(const GpuCounterQueryWin32&) = delete;

    GpuCounterQueryWin32();
    ~GpuCounterQueryWin32();

    [[nodiscard]] GpuCounterQueryState GetState() const noexcept;

    //! @return false if there is no recent measurement (the record is then cleared)
    bool TryGetUsage(GpuUsageRecord& rUsageRecord) const noexcept;
    //! @return false if there is no recent measurement (the record is then cleared)
    bool TryGetMemoryUsage(GpuMemoryUsageRecord& rUsageRecord) const noexcept;

  private:
    void Run() noexcept;
    void RunQuery();
    void PublishNotSupported(const uint32_t status) noexcept;
    //! @return false if the thread was asked to stop
    bool WaitForNextCollect();
  };
}

#endif

#endif
