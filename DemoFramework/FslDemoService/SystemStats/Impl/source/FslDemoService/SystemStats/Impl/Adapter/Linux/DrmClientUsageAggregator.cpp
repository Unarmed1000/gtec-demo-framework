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


#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/Linux/DrmClientUsageAggregator.hpp>
#include <algorithm>

namespace Fsl
{
  void DrmClientUsageAggregator::BeginSample() noexcept
  {
    m_previous = m_current;
    m_current = {};
    m_clientCount = 0;
    m_hasMemory = false;
    m_memoryBytes = 0;
  }


  void DrmClientUsageAggregator::AddClient(const DrmFdInfo& info) noexcept
  {
    if (!info.IsDrmClient)
    {
      return;
    }
    for (uint32_t i = 0; i < m_clientCount; ++i)
    {
      if (m_clientIds[i] == info.ClientId)
      {
        return;
      }
    }
    if (m_clientCount >= MaxClients)
    {
      return;
    }
    m_clientIds[m_clientCount] = info.ClientId;
    ++m_clientCount;

    if (info.HasMemory)
    {
      m_hasMemory = true;
      m_memoryBytes += info.MemoryBytes;
    }

    // The work of a engine is the sum over the clients of the process
    const uint32_t engineCount = std::min(info.EngineCount, DrmFdInfo::MaxEngines);
    for (uint32_t engineIndex = 0; engineIndex < engineCount; ++engineIndex)
    {
      const DrmFdInfo::Engine& engine = info.Engines[engineIndex];
      uint32_t dstIndex = 0;
      while (dstIndex < m_current.EngineCount && m_current.Engines[dstIndex].GetName() != engine.GetName())
      {
        ++dstIndex;
      }
      if (dstIndex < m_current.EngineCount)
      {
        m_current.Engines[dstIndex].BusyNanoseconds += engine.BusyNanoseconds;
      }
      else if (m_current.EngineCount < DrmFdInfo::MaxEngines)
      {
        m_current.Engines[m_current.EngineCount] = engine;
        ++m_current.EngineCount;
      }
    }
  }


  void DrmClientUsageAggregator::EndSample(const TickCount sampleTime) noexcept
  {
    m_current.IsValid = true;
    m_current.Time = sampleTime;

    m_hasUsage = false;
    m_usagePercentage = 0.0f;
    if (!m_previous.IsValid)
    {
      return;
    }
    const TimeSpan elapsed = m_current.Time - m_previous.Time;
    if (elapsed.Ticks() <= 0)
    {
      return;
    }
    const double elapsedNanoseconds = static_cast<double>(elapsed.Ticks()) * static_cast<double>(TickCount::NanoSecondsPerTick);

    double busiest = 0.0;
    for (uint32_t i = 0; i < m_current.EngineCount; ++i)
    {
      const DrmFdInfo::Engine& engine = m_current.Engines[i];
      for (uint32_t previousIndex = 0; previousIndex < m_previous.EngineCount; ++previousIndex)
      {
        const DrmFdInfo::Engine& previousEngine = m_previous.Engines[previousIndex];
        // A busy time that went back means a client was closed, so that engine has nothing to compare with this time
        if (previousEngine.GetName() == engine.GetName() && engine.BusyNanoseconds >= previousEngine.BusyNanoseconds)
        {
          const auto busyNanoseconds = static_cast<double>(engine.BusyNanoseconds - previousEngine.BusyNanoseconds);
          busiest = std::max(busiest, (busyNanoseconds * 100.0) / elapsedNanoseconds);
          m_hasUsage = true;
        }
      }
    }
    m_usagePercentage = static_cast<float>(std::min(busiest, 100.0));
  }


  bool DrmClientUsageAggregator::TryGetUsagePercentage(float& rPercentage) const noexcept
  {
    rPercentage = m_hasUsage ? m_usagePercentage : 0.0f;
    return m_hasUsage;
  }


  bool DrmClientUsageAggregator::TryGetMemoryBytes(uint64_t& rBytes) const noexcept
  {
    rBytes = m_hasMemory ? m_memoryBytes : 0u;
    return m_hasMemory;
  }
}
