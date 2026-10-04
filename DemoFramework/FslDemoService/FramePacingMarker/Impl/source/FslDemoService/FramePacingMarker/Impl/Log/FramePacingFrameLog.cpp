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


#include <FslBase/Log/IO/FmtPath.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingFrameLog.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingLogFileSink.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingLogFormatter.hpp>
#include <fmt/format.h>
#include <exception>
#include <iterator>
#include <utility>

namespace Fsl
{
  FramePacingFrameLog::FramePacingFrameLog(std::unique_ptr<IFramePacingLogSink> sink, const bool useThread)
    : m_writer(std::move(sink), useThread)
  {
    m_text.reserve(4096);
    FramePacingLogFormatter::AppendEventsHeader(m_text);
    m_writer.Append(FramePacingLogStream::Events, m_text);
    AddFact("formatVersion", fmt::format("{}", FramePacingLogFormatter::FormatVersion));
  }


  FramePacingFrameLog::~FramePacingFrameLog()
  {
    Close();
  }


  std::unique_ptr<FramePacingFrameLog> FramePacingFrameLog::TryCreate(const IO::Path& framesPath)
  {
    try
    {
      const IO::Path eventsPath = FramePacingLogFileSink::ToEventsPath(framesPath);
      auto log = std::make_unique<FramePacingFrameLog>(std::make_unique<FramePacingLogFileSink>(framesPath, eventsPath));
      FSLLOG3_INFO("FramePacing: the frames are logged to '{}', the events to '{}'", framesPath, eventsPath);
      return log;
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_ERROR("FramePacing: the frames are not logged: {}", ex.what());
      return nullptr;
    }
  }


  FramePacingLogColumn FramePacingFrameLog::RegisterColumn(const std::string_view name, const FramePacingLogUnit unit,
                                                           const std::string_view description)
  {
    const FramePacingLogColumn column = m_table.RegisterColumn(name, unit, description);
    FSLLOG3_WARNING_IF(!column.IsValid(), "FramePacing: the log column '{}' could not be added (a bad name, too many columns, or too late)", name);
    return column;
  }


  void FramePacingFrameLog::BeginFrame(const uint64_t frameIndex)
  {
    if (m_isClosed)
    {
      return;
    }
    m_frameIndex = frameIndex;
    if (m_table.BeginFrame(frameIndex, m_closedRow))
    {
      WriteRow(m_closedRow);
    }
  }


  void FramePacingFrameLog::AddEvent(const std::string_view name, const std::string_view details)
  {
    if (m_isClosed)
    {
      return;
    }
    m_text.clear();
    FramePacingLogFormatter::AppendEvent(m_text, m_frameIndex, m_timer.GetTimestamp(), name, details);
    m_writer.Append(FramePacingLogStream::Events, m_text);
  }


  void FramePacingFrameLog::AddFact(const std::string_view key, const std::string_view value)
  {
    AddEvent("fact", fmt::format("{}={}", key, value));
  }


  void FramePacingFrameLog::Close() noexcept
  {
    if (m_isClosed)
    {
      return;
    }
    try
    {
      while (m_table.TryCloseOldest(m_closedRow))
      {
        WriteRow(m_closedRow);
      }
      // A log without a frame still gets its header
      WriteHeaderIfNeeded();
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_ERROR("FramePacing: the last rows of the log could not be written: {}", ex.what());
    }
    m_isClosed = true;
    m_writer.Close();
    FSLLOG3_ERROR_IF(m_writer.HasFailed(), "FramePacing: writing the log failed, it is incomplete");
  }


  void FramePacingFrameLog::WriteHeaderIfNeeded()
  {
    if (m_headerWritten)
    {
      return;
    }
    m_headerWritten = true;
    // The columns are part of the first line, so none can be added from now on
    m_table.LockColumns();

    m_text.clear();
    FramePacingLogFormatter::AppendFramesHeader(m_text, m_table.GetColumns());
    m_writer.Append(FramePacingLogStream::Frames, m_text);

    // What the columns are is written to the events, so the frames file stays a plain table
    for (const FramePacingLogColumnInfo& column : m_table.GetColumns())
    {
      AddEvent("column",
               fmt::format("name={};unit={};description={}", column.Name, FramePacingLogFormatter::ToString(column.Unit), column.Description));
    }
  }


  void FramePacingFrameLog::WriteRow(const FramePacingLogRow& row)
  {
    WriteHeaderIfNeeded();
    m_text.clear();
    FramePacingLogFormatter::AppendRow(m_text, row, m_table.GetColumns());
    m_writer.Append(FramePacingLogStream::Frames, m_text);
  }
}
