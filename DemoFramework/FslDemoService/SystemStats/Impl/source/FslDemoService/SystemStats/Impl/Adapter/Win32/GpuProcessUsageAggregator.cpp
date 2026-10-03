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


#include <FslDemoService/SystemStats/Impl/Adapter/Win32/GpuProcessUsageAggregator.hpp>
#include <algorithm>
#include <cmath>

namespace Fsl
{
  namespace
  {
    constexpr std::string_view ProcessPrefix("pid_");
  }


  GpuProcessUsageAggregator::GpuProcessUsageAggregator(const uint32_t processId) noexcept
    : m_processId(processId)
  {
  }


  bool GpuProcessUsageAggregator::IsProcessInstance(const std::string_view instanceName, const uint32_t processId) noexcept
  {
    if (!instanceName.starts_with(ProcessPrefix))
    {
      return false;
    }
    // The digits have to be the whole process id, so "pid_12_" is not a instance of process 123
    std::size_t index = ProcessPrefix.size();
    uint64_t value = 0;
    std::size_t digits = 0;
    while (index < instanceName.size() && instanceName[index] >= '0' && instanceName[index] <= '9' && digits < 10u)
    {
      value = (value * 10u) + static_cast<uint64_t>(instanceName[index] - '0');
      ++index;
      ++digits;
    }
    return digits > 0u && index < instanceName.size() && instanceName[index] == '_' && value == processId;
  }


  void GpuProcessUsageAggregator::Clear() noexcept
  {
    m_hasUsage = false;
    m_usagePercentage = 0.0;
    m_hasMemoryUsage = false;
    m_dedicatedBytes = 0;
    m_sharedBytes = 0;
  }


  void GpuProcessUsageAggregator::AddEngineUtilization(const std::string_view instanceName, const double percentage) noexcept
  {
    if (!IsProcessInstance(instanceName, m_processId) || !std::isfinite(percentage) || percentage < 0.0)
    {
      return;
    }
    m_usagePercentage = std::max(m_usagePercentage, std::min(percentage, 100.0));
    m_hasUsage = true;
  }


  void GpuProcessUsageAggregator::AddDedicatedUsage(const std::string_view instanceName, const uint64_t bytes) noexcept
  {
    if (IsProcessInstance(instanceName, m_processId))
    {
      m_dedicatedBytes += bytes;
      m_hasMemoryUsage = true;
    }
  }


  void GpuProcessUsageAggregator::AddSharedUsage(const std::string_view instanceName, const uint64_t bytes) noexcept
  {
    if (IsProcessInstance(instanceName, m_processId))
    {
      m_sharedBytes += bytes;
      m_hasMemoryUsage = true;
    }
  }


  bool GpuProcessUsageAggregator::TryGetUsagePercentage(float& rPercentage) const noexcept
  {
    rPercentage = m_hasUsage ? static_cast<float>(m_usagePercentage) : 0.0f;
    return m_hasUsage;
  }


  bool GpuProcessUsageAggregator::TryGetMemoryUsage(uint64_t& rDedicatedBytes, uint64_t& rSharedBytes) const noexcept
  {
    rDedicatedBytes = m_hasMemoryUsage ? m_dedicatedBytes : 0u;
    rSharedBytes = m_hasMemoryUsage ? m_sharedBytes : 0u;
    return m_hasMemoryUsage;
  }
}
