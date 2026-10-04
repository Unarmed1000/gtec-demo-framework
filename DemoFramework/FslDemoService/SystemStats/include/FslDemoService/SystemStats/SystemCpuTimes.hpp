#ifndef FSLDEMOSERVICE_SYSTEMSTATS_SYSTEMCPUTIMES_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_SYSTEMCPUTIMES_HPP
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


#include <FslBase/Time/TickCount.hpp>
#include <cstdint>

namespace Fsl
{
  //! How long the CPUs of the system and the CPUs for this process have worked since the system respectively the process started.
  //!
  //! These are the counters of the operating system as they are, in 100ns ticks. They only grow, so the load between two moments is the
  //! difference of two reads: (kernel + user) / (idle + kernel + user) of the system is how busy the machine was, with all CPUs counted.
  struct SystemCpuTimes
  {
    //! When the counters were read (a HighResolutionTimer timestamp)
    TickCount Timer;
    //! The time the CPUs of the system were idle (summed over the CPUs)
    uint64_t SystemIdleTicks{0};
    //! The time the CPUs of the system ran kernel code, the idle time not included
    uint64_t SystemKernelTicks{0};
    //! The time the CPUs of the system ran user code
    uint64_t SystemUserTicks{0};
    //! The time this process ran kernel code (summed over its threads)
    uint64_t ProcessKernelTicks{0};
    //! The time this process ran user code (summed over its threads)
    uint64_t ProcessUserTicks{0};

    constexpr bool operator==(const SystemCpuTimes& rhs) const noexcept = default;
  };
}

#endif
