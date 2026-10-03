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

#include <FslBase/IO/File.hpp>
#include <FslBase/Log/IO/FmtPath.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <Shared/FramePacing/SamplePresentLog.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <exception>
#include <iterator>
#include <utility>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! A value arrives a few frames after the frame it belongs to, so only the last rows are searched
      constexpr std::size_t MaxSearchRows = 256;
      //! The rows the log makes room for when it is created, so the first frames do not allocate
      constexpr std::size_t InitialRows = 4096;
    }

    constexpr const char* const CsvHeader =
      "frameIndex,presentId,cpuStartTicks,endFrameTicks,acquireCallTicks,acquireReturnTicks,presentCallTicks,presentReturnTicks,"
      "queueOperationsEndTicks,firstPixelOutTicks,refreshDurationNs,swapInterval,intendedDisplayTicks,animationTimeTicks,change,imageIndex,"
      "resultReadAtPresentId\n";

    const char* ToString(const SamplePacerChange change) noexcept
    {
      switch (change)
      {
      case SamplePacerChange::Slower:
        return "slower";
      case SamplePacerChange::Faster:
        return "faster";
      case SamplePacerChange::Unchanged:
      default:
        return "unchanged";
      }
    }

    template <typename TIterator>
    void AppendTicks(TIterator& rDst, const std::optional<TickCount>& value)
    {
      if (value.has_value())
      {
        rDst = fmt::format_to(rDst, "{}", value->Ticks());
      }
      rDst = fmt::format_to(rDst, ",");
    }
  }


  SamplePresentLog::SamplePresentLog(IO::Path path, const std::size_t maxRows)
    : m_path(std::move(path))
    , m_maxRows(m_path.IsEmpty() ? 0u : maxRows)
  {
    m_rows.reserve(std::min(m_maxRows, LocalConfig::InitialRows));
  }


  void SamplePresentLog::AddFrame(const SamplePresentLogFrame& frame)
  {
    if (frame.PresentId == 0u || m_rows.size() >= m_maxRows)
    {
      return;
    }
    Row row;
    row.PresentId = frame.PresentId;
    row.CpuStartTime = frame.CpuStartTime;
    row.RefreshDuration = frame.RefreshDuration;
    row.AnimationTime = frame.AnimationTime;
    row.Schedule = frame.Schedule;
    m_rows.push_back(row);
  }


  void SamplePresentLog::SetFrameIndex(const uint64_t presentId, const uint64_t frameIndex) noexcept
  {
    Row* const pRow = TryGetRow(presentId);
    if (pRow != nullptr)
    {
      pRow->FrameIndex = frameIndex;
    }
  }


  void SamplePresentLog::SetEndFrameTime(const uint64_t presentId, const TickCount endFrameTime) noexcept
  {
    Row* const pRow = TryGetRow(presentId);
    if (pRow != nullptr)
    {
      pRow->EndFrameTime = endFrameTime;
    }
  }


  void SamplePresentLog::SetPresentCalls(const uint64_t presentId, const uint32_t imageIndex, const TickCount acquireCallTime,
                                         const TickCount acquireReturnTime, const TickCount presentCallTime,
                                         const TickCount presentReturnTime) noexcept
  {
    Row* const pRow = TryGetRow(presentId);
    if (pRow != nullptr)
    {
      pRow->ImageIndex = imageIndex;
      pRow->AcquireCallTime = acquireCallTime;
      pRow->AcquireReturnTime = acquireReturnTime;
      pRow->PresentCallTime = presentCallTime;
      pRow->PresentReturnTime = presentReturnTime;
    }
  }


  void SamplePresentLog::SetPresentTiming(const uint64_t presentId, const std::optional<TickCount> queueOperationsEndTime,
                                          const std::optional<TickCount> displayTime, const uint64_t readAtPresentId) noexcept
  {
    Row* const pRow = TryGetRow(presentId);
    if (pRow != nullptr)
    {
      pRow->QueueOperationsEndTime = queueOperationsEndTime;
      pRow->DisplayTime = displayTime;
      pRow->ResultReadAtPresentId = readAtPresentId;
    }
  }


  std::string SamplePresentLog::ToCsv() const
  {
    std::string content(CsvHeader);
    auto dst = std::back_inserter(content);
    for (const Row& row : m_rows)
    {
      if (row.FrameIndex.has_value())
      {
        dst = fmt::format_to(dst, "{}", row.FrameIndex.value());
      }
      dst = fmt::format_to(dst, ",{},{},", row.PresentId, row.CpuStartTime.Ticks());
      AppendTicks(dst, row.EndFrameTime);
      AppendTicks(dst, row.AcquireCallTime);
      AppendTicks(dst, row.AcquireReturnTime);
      AppendTicks(dst, row.PresentCallTime);
      AppendTicks(dst, row.PresentReturnTime);
      AppendTicks(dst, row.QueueOperationsEndTime);
      AppendTicks(dst, row.DisplayTime);
      if (row.RefreshDuration.Ticks() > 0)
      {
        dst = fmt::format_to(dst, "{}", row.RefreshDuration.Ticks() * TickCount::NanoSecondsPerTick);
      }
      dst = fmt::format_to(dst, ",");
      if (row.Schedule.has_value())
      {
        dst = fmt::format_to(dst, "{},{},{},{},", row.Schedule->SwapInterval, row.Schedule->IntendedDisplayTime.Ticks(), row.AnimationTime.Ticks(),
                             ToString(row.Schedule->Change));
      }
      else
      {
        dst = fmt::format_to(dst, ",,{},,", row.AnimationTime.Ticks());
      }
      if (row.ImageIndex.has_value())
      {
        dst = fmt::format_to(dst, "{}", row.ImageIndex.value());
      }
      dst = fmt::format_to(dst, ",");
      if (row.ResultReadAtPresentId.has_value())
      {
        dst = fmt::format_to(dst, "{}", row.ResultReadAtPresentId.value());
      }
      dst = fmt::format_to(dst, "\n");
    }
    return content;
  }


  bool SamplePresentLog::TrySave() const noexcept
  {
    if (!IsEnabled())
    {
      return false;
    }
    try
    {
      IO::File::WriteAllText(m_path, ToCsv());
      FSLLOG3_INFO("Present log: {} frames written to '{}'", m_rows.size(), m_path);
      return true;
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_ERROR("Present log: failed to write '{}': {}", m_path, ex.what());
      return false;
    }
  }


  SamplePresentLog::Row* SamplePresentLog::TryGetRow(const uint64_t presentId) noexcept
  {
    if (presentId == 0u)
    {
      return nullptr;
    }
    const std::size_t searchRows = std::min(m_rows.size(), LocalConfig::MaxSearchRows);
    for (std::size_t i = 0; i < searchRows; ++i)
    {
      Row& rRow = m_rows[m_rows.size() - 1u - i];
      if (rRow.PresentId == presentId)
      {
        return &rRow;
      }
    }
    return nullptr;
  }
}
