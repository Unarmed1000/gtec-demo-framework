#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_IGPUSTATSADAPTER_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_IGPUSTATSADAPTER_HPP
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


#include <FslDemoService/SystemStats/GpuMemoryUsageRecord.hpp>
#include <FslDemoService/SystemStats/GpuUsageRecord.hpp>

namespace Fsl
{
  //! What a platform knows about the GPU usage of the application.
  //! @note A adapter is created when the GPU usage is asked for the first time. The methods are called from the thread of the service and
  //!       must return at once: a adapter that needs time to measure does so on a thread of its own.
  class IGpuStatsAdapter
  {
  public:
    virtual ~IGpuStatsAdapter() = default;

    //! @brief Get the load of the busiest GPU engine the application uses.
    //! @return false if not available (yet).
    virtual bool TryGetApplicationGpuUsage(GpuUsageRecord& rUsageRecord) const = 0;

    //! @brief Get the GPU memory the application uses.
    //! @return false if not available (yet).
    virtual bool TryGetApplicationGpuMemoryUsage(GpuMemoryUsageRecord& rUsageRecord) const = 0;
  };
}

#endif
