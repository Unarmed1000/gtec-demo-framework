#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_SYSTEMCPUTIMESREADER_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_SYSTEMCPUTIMESREADER_HPP
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


#include <FslDemoService/SystemStats/SystemCpuTimes.hpp>
#include <cstdint>
#include <string_view>

//! Reads the CPU time counters of the operating system.
namespace Fsl::SystemCpuTimesReader
{
  //! @brief Read the counters now. The time of the record is not set.
  //! @return false if the platform has no such counters (the record is then cleared).
  bool TryRead(SystemCpuTimes& rTimes) noexcept;

  //! @brief Parse the first line of the Linux /proc/stat ("cpu  user nice system idle iowait irq softirq steal ...") into the system times.
  //!        It has no platform dependency, so it can be tested everywhere.
  //! @param ticksPerJiffy the number of 100ns ticks of the unit the line counts in (100000 for the usual 100 per second).
  //! @return false if the line is not the cpu line or has too few numbers (the system times are then cleared).
  bool TryParseProcStatCpuLine(const std::string_view line, const uint64_t ticksPerJiffy, SystemCpuTimes& rTimes) noexcept;
}

#endif
