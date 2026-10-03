#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_WIN32_GPUPROCESSUSAGEAGGREGATOR_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_WIN32_GPUPROCESSUSAGEAGGREGATOR_HPP
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


#include <cstdint>
#include <string_view>

namespace Fsl
{
  //! Turns the instances of the Windows GPU performance counters into the GPU usage of one process.
  //!
  //! The counters have one instance per process and GPU engine ("pid_1234_luid_0x00000000_0x00012014_phys_0_eng_0_engtype_3D") or per
  //! process and GPU ("pid_1234_luid_0x00000000_0x00012014_phys_0"). The usage of a process is the load of its busiest engine, which is
  //! what the Windows Task Manager shows, and its memory is the sum over the GPUs.
  //!
  //! This has no platform dependency, so it can be tested everywhere.
  class GpuProcessUsageAggregator final
  {
    uint32_t m_processId{0};
    bool m_hasUsage{false};
    double m_usagePercentage{0.0};
    bool m_hasMemoryUsage{false};
    uint64_t m_dedicatedBytes{0};
    uint64_t m_sharedBytes{0};

  public:
    explicit GpuProcessUsageAggregator(const uint32_t processId) noexcept;

    //! @return true if the instance belongs to the process: its name starts with "pid_<processId>_"
    [[nodiscard]] static bool IsProcessInstance(const std::string_view instanceName, const uint32_t processId) noexcept;

    //! @brief Forget everything that was added
    void Clear() noexcept;

    //! @brief Add the load of a GPU engine in percent. A instance of another process and a value that is not a number are ignored.
    void AddEngineUtilization(const std::string_view instanceName, const double percentage) noexcept;

    //! @brief Add the dedicated GPU memory of a GPU in bytes. A instance of another process is ignored.
    void AddDedicatedUsage(const std::string_view instanceName, const uint64_t bytes) noexcept;

    //! @brief Add the shared GPU memory of a GPU in bytes. A instance of another process is ignored.
    void AddSharedUsage(const std::string_view instanceName, const uint64_t bytes) noexcept;

    //! @brief Get the load of the busiest engine (0-100).
    //! @return false if no engine of the process was added.
    [[nodiscard]] bool TryGetUsagePercentage(float& rPercentage) const noexcept;

    //! @brief Get the memory summed over the GPUs.
    //! @return false if no memory of the process was added.
    [[nodiscard]] bool TryGetMemoryUsage(uint64_t& rDedicatedBytes, uint64_t& rSharedBytes) const noexcept;
  };
}

#endif
