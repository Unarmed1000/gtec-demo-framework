#ifndef FSLDEMOSERVICE_SYSTEMSTATS_CONTROL_ISYSTEMSTATSSERVICECONTROL_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_CONTROL_ISYSTEMSTATSSERVICECONTROL_HPP
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

namespace Fsl
{
  //! For the part of the framework that owns the graphics API: it can tell the system stats service what the API knows, which the service
  //! uses where the operating system has no number of its own.
  class ISystemStatsServiceControl
  {
  public:
    virtual ~ISystemStatsServiceControl() = default;

    //! @brief Check if the GPU memory usage of the graphics API would be used: somebody asked for the GPU memory usage recently and the
    //!        operating system did not have it. Use it to skip the work of finding the usage out.
    [[nodiscard]] virtual bool IsApplicationGpuMemoryUsageWanted() const noexcept = 0;

    //! @brief Tell the service how much GPU memory the application uses according to the graphics API (VK_EXT_memory_budget for example).
    //!        Call it about once a second, a value that is not refreshed is dropped after a few seconds.
    //! @param dedicatedBytes the memory of the GPU itself the application uses.
    //! @param sharedBytes the system memory the GPU uses for the application.
    virtual void SetApplicationGpuMemoryUsage(const uint64_t dedicatedBytes, const uint64_t sharedBytes) noexcept = 0;
  };
}

#endif
