#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_LINUX_GPUSTATSADAPTERLINUX_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_LINUX_GPUSTATSADAPTERLINUX_HPP
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
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/IGpuStatsAdapter.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/Linux/DrmClientUsageAggregator.hpp>

namespace Fsl
{
  //! The GPU usage of the application according to the DRM clients of the process (/proc/self/fdinfo, "DRM client usage stats").
  //! Only some drivers report it (Intel, AMD and a number of the ARM GPU drivers; the NVIDIA proprietary driver does not), on the others
  //! nothing is available. It is read at most once a second and only the file descriptors of GPU devices are looked at.
  //! @note The memory is reported as dedicated memory: the kernel does not tell the memory of the GPU from system memory.
  class GpuStatsAdapterLinux final : public IGpuStatsAdapter
  {
    HighResolutionTimer m_timer;
    mutable DrmClientUsageAggregator m_aggregator;
    mutable TickCount m_lastSampleTime;
    mutable bool m_hasSample{false};
    //! False once reading failed in a way that will not get better
    mutable bool m_enabled{true};

  public:
    GpuStatsAdapterLinux();

    bool TryGetApplicationGpuUsage(GpuUsageRecord& rUsageRecord) const final;
    bool TryGetApplicationGpuMemoryUsage(GpuMemoryUsageRecord& rUsageRecord) const final;

  private:
    void SampleIfNeeded() const noexcept;
    void Sample(const TickCount currentTime) const;
  };
}

#endif

#endif
