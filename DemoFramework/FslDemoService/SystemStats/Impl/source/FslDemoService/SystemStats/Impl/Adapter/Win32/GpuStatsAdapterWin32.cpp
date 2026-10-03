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
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/Win32/GpuStatsAdapterWin32.hpp>
#include "GpuCounterQueryWin32.hpp"

namespace Fsl
{
  GpuStatsAdapterWin32::GpuStatsAdapterWin32()
    : m_query(std::make_unique<GpuCounterQueryWin32>())
  {
  }


  GpuStatsAdapterWin32::~GpuStatsAdapterWin32() = default;


  bool GpuStatsAdapterWin32::TryGetApplicationGpuUsage(GpuUsageRecord& rUsageRecord) const
  {
    LogStateOnce();
    return m_query->TryGetUsage(rUsageRecord);
  }


  bool GpuStatsAdapterWin32::TryGetApplicationGpuMemoryUsage(GpuMemoryUsageRecord& rUsageRecord) const
  {
    LogStateOnce();
    return m_query->TryGetMemoryUsage(rUsageRecord);
  }


  void GpuStatsAdapterWin32::LogStateOnce() const noexcept
  {
    if (m_hasLoggedState)
    {
      return;
    }
    // The thread of the query does not log, so what it found is logged here once it is known
    const GpuCounterQueryState state = m_query->GetState();
    switch (state.Setup)
    {
    case GpuCounterQuerySetup::Ready:
      m_hasLoggedState = true;
      FSLLOG3_VERBOSE("GPU stats: the performance counters were set up in {:.1f} ms on a thread of their own", state.SetupTime.TotalMilliseconds());
      break;
    case GpuCounterQuerySetup::NotSupported:
      m_hasLoggedState = true;
      FSLLOG3_VERBOSE("GPU stats: not available, the GPU performance counters could not be set up ({:#x})", state.SetupStatus);
      break;
    case GpuCounterQuerySetup::Pending:
    default:
      break;
    }
  }
}

#endif
