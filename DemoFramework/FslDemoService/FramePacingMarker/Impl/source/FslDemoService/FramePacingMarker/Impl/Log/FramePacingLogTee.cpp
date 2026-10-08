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

#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingFrameLog.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingLogTee.hpp>
#include <FslDemoService/Trace/Control/ITraceServiceControl.hpp>
#include <FslDemoService/Trace/ITraceService.hpp>
#include <utility>

namespace Fsl
{
  namespace
  {
    constexpr TraceUnit ToTraceUnit(const FramePacingLogUnit unit) noexcept
    {
      switch (unit)
      {
      case FramePacingLogUnit::Ticks:
        return TraceUnit::Ticks;
      case FramePacingLogUnit::DurationTicks:
        return TraceUnit::DurationTicks;
      case FramePacingLogUnit::Nanoseconds:
        return TraceUnit::Nanoseconds;
      case FramePacingLogUnit::Count:
        return TraceUnit::Count;
      case FramePacingLogUnit::Id:
        return TraceUnit::Id;
      case FramePacingLogUnit::Flag:
        return TraceUnit::Flag;
      case FramePacingLogUnit::Pixels:
        return TraceUnit::Pixels;
      case FramePacingLogUnit::Code:
        return TraceUnit::Code;
      }
      return TraceUnit::Count;
    }
  }


  FramePacingLogTee::FramePacingLogTee(std::unique_ptr<FramePacingFrameLog> log, std::shared_ptr<ITraceService> trace,
                                       std::shared_ptr<ITraceServiceControl> traceControl)
    : m_log(std::move(log))
    , m_trace(std::move(trace))
    , m_traceControl(std::move(traceControl))
  {
  }


  FramePacingLogTee::~FramePacingLogTee()
  {
    Close();
  }


  std::unique_ptr<FramePacingLogTee> FramePacingLogTee::TryCreate(const IO::Path& logPath, const std::shared_ptr<ITraceService>& trace,
                                                                  const std::shared_ptr<ITraceServiceControl>& traceControl)
  {
    std::unique_ptr<FramePacingFrameLog> log;
    if (!logPath.IsEmpty())
    {
      log = FramePacingFrameLog::TryCreate(logPath);
    }
    const bool hasTrace = trace && traceControl && trace->IsEnabled();
    if (!log && !hasTrace)
    {
      return nullptr;
    }
    return std::make_unique<FramePacingLogTee>(std::move(log), hasTrace ? trace : nullptr, hasTrace ? traceControl : nullptr);
  }


  FramePacingLogColumn FramePacingLogTee::RegisterColumn(const std::string_view name, const FramePacingLogUnit unit,
                                                         const std::string_view description)
  {
    for (std::size_t i = 0; i < m_columns.size(); ++i)
    {
      if (m_columns[i].Name == name)
      {
        return FramePacingLogColumn(static_cast<uint32_t>(i + 1u));
      }
    }
    ColumnRecord record;
    record.Name = std::string(name);
    if (m_log)
    {
      record.LogColumn = m_log->RegisterColumn(name, unit, description);
    }
    if (m_trace)
    {
      record.Value = m_trace->RegisterValue(name, ToTraceUnit(unit), description);
    }
    if (!record.LogColumn.IsValid() && !record.Value.IsValid())
    {
      // Neither place took it, and each said why
      return {};
    }
    m_columns.push_back(std::move(record));
    return FramePacingLogColumn(static_cast<uint32_t>(m_columns.size()));
  }


  void FramePacingLogTee::BeginFrame(const uint64_t frameIndex, const uint32_t runId)
  {
    m_frameIndex = frameIndex;
    if (m_log)
    {
      m_log->BeginFrame(frameIndex);
    }
    if (m_traceControl)
    {
      m_traceControl->BeginFrame(TraceFrameIndex(frameIndex), TraceRunId(runId));
    }
  }


  void FramePacingLogTee::SetTraceFrameBounds(const std::string_view beginColumnName, const std::string_view endColumnName)
  {
    if (m_trace && m_traceControl)
    {
      m_traceControl->SetFrameBounds(m_trace->FindValue(beginColumnName), m_trace->FindValue(endColumnName));
    }
  }


  void FramePacingLogTee::SetInt64At(const uint64_t frameIndex, const FramePacingLogColumn column, const int64_t value) noexcept
  {
    if (!column.IsValid() || column.Value > m_columns.size())
    {
      return;
    }
    const ColumnRecord& record = m_columns[column.Value - 1u];
    if (m_log)
    {
      m_log->SetInt64At(frameIndex, record.LogColumn, value);
    }
    if (m_trace)
    {
      m_trace->SetInt64At(TraceFrameIndex(frameIndex), record.Value, value);
    }
  }


  void FramePacingLogTee::SetUInt64At(const uint64_t frameIndex, const FramePacingLogColumn column, const uint64_t value) noexcept
  {
    if (!column.IsValid() || column.Value > m_columns.size())
    {
      return;
    }
    const ColumnRecord& record = m_columns[column.Value - 1u];
    if (m_log)
    {
      m_log->SetUInt64At(frameIndex, record.LogColumn, value);
    }
    if (m_trace)
    {
      m_trace->SetUInt64At(TraceFrameIndex(frameIndex), record.Value, value);
    }
  }


  void FramePacingLogTee::AddEvent(const std::string_view name, const std::string_view details)
  {
    if (m_log)
    {
      m_log->AddEvent(name, details);
    }
    if (m_trace)
    {
      m_trace->AddEvent(name, details);
    }
  }


  void FramePacingLogTee::AddFact(const std::string_view key, const std::string_view value)
  {
    if (m_log)
    {
      m_log->AddFact(key, value);
    }
    if (m_trace)
    {
      m_trace->SetFact(key, value);
    }
  }


  void FramePacingLogTee::Close() noexcept
  {
    if (m_log)
    {
      m_log->Close();
    }
  }
}
