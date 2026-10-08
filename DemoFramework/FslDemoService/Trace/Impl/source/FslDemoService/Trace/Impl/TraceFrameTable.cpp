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

#include <FslDemoService/Trace/Impl/TraceFrameTable.hpp>
#include <algorithm>

namespace Fsl
{
  TraceValue TraceFrameTable::RegisterValue(const std::string_view name, const TraceUnit unit, const std::string_view description)
  {
    if (!IsValidValueName(name))
    {
      return {};
    }
    const TraceValue existing = FindValue(name);
    if (existing.IsValid())
    {
      return existing;
    }
    if (m_valuesLocked || m_values.size() >= TraceFrameRow::MaxValues)
    {
      return {};
    }
    TraceValueInfo info;
    info.Name = std::string(name);
    info.Unit = unit;
    info.Description = std::string(description);
    m_values.push_back(std::move(info));
    return TraceValue(static_cast<uint32_t>(m_values.size()));
  }


  TraceValue TraceFrameTable::FindValue(const std::string_view name) const noexcept
  {
    for (std::size_t i = 0; i < m_values.size(); ++i)
    {
      if (m_values[i].Name == name)
      {
        return TraceValue(static_cast<uint32_t>(i + 1u));
      }
    }
    return {};
  }


  bool TraceFrameTable::IsValidValueName(const std::string_view name) noexcept
  {
    return !name.empty() &&
           std::all_of(name.begin(), name.end(), [](const char ch)
                       { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '.'; });
  }


  bool TraceFrameTable::BeginFrame(const uint64_t frameIndex, const uint32_t runId, TraceFrameRow& rClosedRow) noexcept
  {
    const auto slot = static_cast<std::size_t>(frameIndex % OpenRowCount);
    const bool wasOpen = m_isOpen[slot];
    if (wasOpen)
    {
      rClosedRow = m_rows[slot];
    }
    TraceFrameRow& rRow = m_rows[slot];
    rRow.FrameIndex = frameIndex;
    rRow.RunId = runId;
    rRow.HasValue.reset();
    m_isOpen[slot] = true;
    m_newestFrameIndex = frameIndex;
    m_hasFrame = true;
    return wasOpen;
  }


  void TraceFrameTable::SetValue(const uint64_t frameIndex, const TraceValue value, const int64_t number) noexcept
  {
    if (!IsValue(value) || !IsOpen(frameIndex))
    {
      return;
    }
    const std::size_t valueIndex = value.Value - 1u;
    TraceFrameRow& rRow = m_rows[static_cast<std::size_t>(frameIndex % OpenRowCount)];
    rRow.Values[valueIndex] = number;
    rRow.HasValue.set(valueIndex);
  }


  bool TraceFrameTable::IsOpen(const uint64_t frameIndex) const noexcept
  {
    const auto slot = static_cast<std::size_t>(frameIndex % OpenRowCount);
    return m_isOpen[slot] && m_rows[slot].FrameIndex == frameIndex;
  }


  bool TraceFrameTable::TryCloseOldest(TraceFrameRow& rClosedRow) noexcept
  {
    if (!m_hasFrame)
    {
      return false;
    }
    // The open rows are the newest frame and the ones before it, so the oldest is found by walking up to the newest
    const uint64_t firstFrameIndex = m_newestFrameIndex >= (OpenRowCount - 1u) ? (m_newestFrameIndex - (OpenRowCount - 1u)) : 0u;
    for (uint64_t frameIndex = firstFrameIndex; frameIndex <= m_newestFrameIndex; ++frameIndex)
    {
      if (IsOpen(frameIndex))
      {
        const auto slot = static_cast<std::size_t>(frameIndex % OpenRowCount);
        rClosedRow = m_rows[slot];
        m_isOpen[slot] = false;
        return true;
      }
    }
    // Rows of frames that are not in order (the frame index went back) are closed in the order of their place
    for (std::size_t slot = 0; slot < OpenRowCount; ++slot)
    {
      if (m_isOpen[slot])
      {
        rClosedRow = m_rows[slot];
        m_isOpen[slot] = false;
        return true;
      }
    }
    return false;
  }
}
