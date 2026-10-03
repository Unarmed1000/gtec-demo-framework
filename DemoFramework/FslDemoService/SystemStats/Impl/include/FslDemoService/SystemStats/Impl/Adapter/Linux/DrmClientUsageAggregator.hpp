#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_LINUX_DRMCLIENTUSAGEAGGREGATOR_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_ADAPTER_LINUX_DRMCLIENTUSAGEAGGREGATOR_HPP
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
#include <FslDemoService/SystemStats/Impl/Adapter/Linux/DrmFdInfo.hpp>
#include <array>
#include <cstdint>

namespace Fsl
{
  //! Turns the DRM clients of a process into its GPU usage: the load of its busiest engine and its memory.
  //!
  //! A engine reports how long it has worked for a client, so the load is the work between two samples divided by the time between them.
  //! Take a sample with BeginSample, AddClient for every DRM client and EndSample. It has no platform dependency.
  class DrmClientUsageAggregator final
  {
    static constexpr uint32_t MaxClients = 32;

    struct Sample
    {
      bool IsValid{false};
      TickCount Time;
      std::array<DrmFdInfo::Engine, DrmFdInfo::MaxEngines> Engines{};
      uint32_t EngineCount{0};
    };

    Sample m_previous;
    Sample m_current;
    std::array<uint64_t, MaxClients> m_clientIds{};
    uint32_t m_clientCount{0};
    bool m_hasMemory{false};
    uint64_t m_memoryBytes{0};
    bool m_hasUsage{false};
    float m_usagePercentage{0.0f};

  public:
    //! @brief Start a new sample.
    void BeginSample() noexcept;

    //! @brief Add a DRM client. A file descriptor that is not a client is ignored, as is a client that was added already (several file
    //!        descriptors can be the same client).
    void AddClient(const DrmFdInfo& info) noexcept;

    //! @brief The sample is complete.
    //! @param sampleTime when the sample was taken.
    void EndSample(const TickCount sampleTime) noexcept;

    //! @brief Get the load of the busiest engine (0-100) between the last two samples.
    //! @return false if there are not two samples yet or no engine was reported.
    [[nodiscard]] bool TryGetUsagePercentage(float& rPercentage) const noexcept;

    //! @brief Get the memory of the clients of the last sample.
    //! @return false if no client reported its memory.
    [[nodiscard]] bool TryGetMemoryBytes(uint64_t& rBytes) const noexcept;
  };
}

#endif
