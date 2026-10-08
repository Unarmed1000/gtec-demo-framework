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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoService/Trace/Impl/TraceLog.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <exception>
#include <span>
#include <utility>

namespace Fsl
{
  TraceLog::TraceLog(std::unique_ptr<ITraceSink> sink, const bool anonymise, const bool useThread)
    : m_writer(std::move(sink), useThread)
    , m_anonymise(anonymise)
    // Room for every begin that can wait and for its end
    , m_zones(MaxWaitingZoneRecords * 2u)
  {
  }


  TraceLog::~TraceLog()
  {
    Close();
  }


  void TraceLog::SetThread(const uint64_t threadId, const std::string_view name)
  {
    if (!m_isClosed)
    {
      m_writer.SetThread(threadId, name);
    }
  }


  void TraceLog::SetProcessName(const std::string_view name)
  {
    if (!m_isClosed)
    {
      m_writer.SetProcessName(name);
    }
  }


  TraceZone TraceLog::RegisterZone(const std::string_view name)
  {
    if (m_isClosed || name.empty())
    {
      return {};
    }
    for (std::size_t i = 0; i < m_zoneNames.size(); ++i)
    {
      if (m_zoneNames[i] == name)
      {
        return TraceZone(static_cast<uint32_t>(i + 1u));
      }
    }
    m_zoneNames.emplace_back(name);
    const auto zone = static_cast<uint32_t>(m_zoneNames.size());
    m_writer.AddZoneName(zone, name);
    return TraceZone(zone);
  }


  void TraceLog::BeginZoneAt(const TraceZone zone, const TickCount time) noexcept
  {
    // A zone that is not kept is counted, so its end is not taken for the end of the zone around it. What is inside a zone that
    // was dropped is dropped as well.
    const bool canKeep = !m_isClosed && zone.IsValid() && zone.Value <= m_zoneNames.size() && m_zoneCount < MaxWaitingZoneRecords;
    if (m_droppedZoneDepth > 0u || !canKeep)
    {
      ++m_droppedZoneDepth;
      ++m_droppedZones;
      return;
    }
    // The times of a thread do not go back
    m_lastZoneTicks = std::max(m_lastZoneTicks, time.Ticks());
    m_zones[m_zoneCount] = TraceZoneRecord{m_lastZoneTicks, zone.Value, true};
    ++m_zoneCount;
    ++m_zoneDepth;
  }


  void TraceLog::EndZoneAt(const TickCount time) noexcept
  {
    if (m_droppedZoneDepth > 0u)
    {
      --m_droppedZoneDepth;
      return;
    }
    if (m_zoneDepth == 0u || m_isClosed)
    {
      return;
    }
    m_lastZoneTicks = std::max(m_lastZoneTicks, time.Ticks());
    // The end of a zone that was kept is always kept: if there is no room left, what waits is handed over first
    if (m_zoneCount >= m_zones.size())
    {
      TryHandOverZones();
    }
    m_zones[m_zoneCount] = TraceZoneRecord{m_lastZoneTicks, 0u, false};
    ++m_zoneCount;
    --m_zoneDepth;
    if (m_zoneCount >= ZoneHandOverCount)
    {
      TryHandOverZones();
    }
  }


  TraceTrack TraceLog::RegisterTrack(const std::string_view name, const TraceTrackKind kind)
  {
    if (m_isClosed || name.empty())
    {
      return {};
    }
    for (std::size_t i = 0; i < m_tracks.size(); ++i)
    {
      if (m_tracks[i].Name == name)
      {
        return TraceTrack(static_cast<uint32_t>(i + 1u));
      }
    }
    if (m_schemaWritten)
    {
      FSLLOG3_WARNING("Trace: the track '{}' could not be added, it is too late", name);
      return {};
    }
    m_tracks.push_back(TraceTrackInfo{std::string(name), kind});
    return TraceTrack(static_cast<uint32_t>(m_tracks.size()));
  }


  TraceValue TraceLog::RegisterValue(const std::string_view name, const TraceUnit unit, const std::string_view description)
  {
    if (m_isClosed)
    {
      return {};
    }
    const TraceValue value = m_table.RegisterValue(name, unit, description);
    FSLLOG3_WARNING_IF(!value.IsValid(), "Trace: the value '{}' could not be added (a bad name, too many values, or too late)", name);
    return value;
  }


  bool TraceLog::DeclareSpan(const std::string_view title, const TraceTrack track, const TraceValue begin, const TraceValue end, const TraceLink link)
  {
    if (m_isClosed || m_schemaWritten || title.empty() || !IsTrack(track) || !m_table.IsTime(begin) || !m_table.IsTime(end))
    {
      FSLLOG3_WARNING_IF(!m_isClosed, "Trace: the span '{}' could not be declared (not a track, not two times, or too late)", title);
      return false;
    }
    m_spans.push_back(TraceSpanInfo{std::string(title), track.Value - 1u, begin.Value - 1u, end.Value - 1u, link});
    return true;
  }


  bool TraceLog::DeclareMark(const std::string_view title, const TraceTrack track, const TraceValue time, const TraceLink link)
  {
    if (m_isClosed || m_schemaWritten || title.empty() || !IsTrack(track) || !m_table.IsTime(time))
    {
      FSLLOG3_WARNING_IF(!m_isClosed, "Trace: the mark '{}' could not be declared (not a track, not a time, or too late)", title);
      return false;
    }
    m_marks.push_back(TraceMarkInfo{std::string(title), track.Value - 1u, time.Value - 1u, link});
    return true;
  }


  bool TraceLog::DeclareCounter(const std::string_view title, const TraceValue value)
  {
    // A moment is not something to draw a graph of
    if (m_isClosed || m_schemaWritten || title.empty() || !m_table.IsValue(value) || m_table.IsTime(value))
    {
      FSLLOG3_WARNING_IF(!m_isClosed, "Trace: the counter '{}' could not be declared (not a value, a time, or too late)", title);
      return false;
    }
    m_counters.push_back(TraceCounterInfo{std::string(title), value.Value - 1u});
    return true;
  }


  void TraceLog::SetFrameBounds(const TraceValue begin, const TraceValue end)
  {
    if (m_isClosed || m_schemaWritten || !m_table.IsTime(begin) || !m_table.IsTime(end))
    {
      return;
    }
    m_frameBegin = begin;
    m_frameEnd = end;
  }


  void TraceLog::BeginFrame(const uint64_t frameIndex, const uint32_t runId)
  {
    if (m_isClosed)
    {
      return;
    }
    HandOverZones();
    m_frameIndex = frameIndex;
    m_runId = runId;
    m_hasFrame = true;
    if (m_table.BeginFrame(frameIndex, runId, m_closedRow))
    {
      WriteRow(m_closedRow);
    }
  }


  void TraceLog::AddEvent(const std::string_view name, const std::string_view details)
  {
    AddRecord(name, details, false);
  }


  void TraceLog::SetFact(const std::string_view key, const std::string_view value)
  {
    if (m_anonymise)
    {
      for (const auto& entry : m_anonymousFacts)
      {
        if (entry.first == key)
        {
          AddRecord(key, entry.second, true);
          return;
        }
      }
    }
    AddRecord(key, value, true);
  }


  void TraceLog::AddAnonymousText(const std::string_view text, const std::string_view replacement)
  {
    m_anonymiser.AddText(text, replacement);
  }


  void TraceLog::AddAnonymousFact(const std::string_view key, const std::string_view replacement)
  {
    if (!key.empty())
    {
      m_anonymousFacts.emplace_back(std::string(key), std::string(replacement));
    }
  }


  void TraceLog::AddAnonymousDirectory(const std::string_view directory, const std::string_view replacement)
  {
    m_anonymiser.AddDirectory(directory, replacement);
  }


  void TraceLog::Close() noexcept
  {
    if (m_isClosed)
    {
      return;
    }
    try
    {
      // A zone that is still open ends here, so every begin in the trace has its end
      const TickCount now = m_timer.GetTimestamp();
      m_droppedZoneDepth = 0;
      for (uint32_t remaining = m_zoneDepth; remaining > 0u; --remaining)
      {
        EndZoneAt(now);
      }
      HandOverZones();
      // A trace without a frame still gets its schema
      WriteSchemaIfNeeded();
      while (m_table.TryCloseOldest(m_closedRow))
      {
        WriteRow(m_closedRow);
      }
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_ERROR("Trace: the last frames of the trace could not be written: {}", ex.what());
    }
    m_isClosed = true;
    m_writer.Close();
    FSLLOG3_ERROR_IF(m_writer.HasFailed(), "Trace: writing the trace failed, it is incomplete");
    FSLLOG3_ERROR_IF(m_handOverFailed, "Trace: zones were lost as there was no memory to hand them over, the trace is incomplete");
  }


  void TraceLog::AddRecord(const std::string_view name, const std::string_view details, const bool isFact)
  {
    if (m_isClosed || name.empty())
    {
      return;
    }
    TraceEventRecord record;
    record.Ticks = m_timer.GetTimestamp().Ticks();
    record.FrameIndex = m_frameIndex;
    record.RunId = m_runId;
    record.HasFrame = m_hasFrame;
    record.IsFact = isFact;
    record.Name = std::string(name);
    record.Details = std::string(details);
    if (m_anonymise)
    {
      m_anonymiser.Apply(record.Details);
    }
    m_writer.AddEvent(std::move(record));
  }


  void TraceLog::TryHandOverZones() noexcept
  {
    try
    {
      HandOverZones();
    }
    catch (const std::exception&)
    {
      // No memory for them: the zones that waited are lost, which is said when the trace is closed
      m_zoneCount = 0;
      m_handOverFailed = true;
    }
  }


  void TraceLog::HandOverZones()
  {
    if (m_zoneCount > 0u)
    {
      const std::size_t count = m_zoneCount;
      m_zoneCount = 0;
      m_writer.AddZones(std::span<const TraceZoneRecord>(m_zones.data(), count));
    }
    if (m_droppedZones != m_reportedDroppedZones)
    {
      // Said when it changes, so a trace that lacks zones says so itself
      m_reportedDroppedZones = m_droppedZones;
      FSLLOG3_VERBOSE("Trace: {} zones were dropped so far", m_droppedZones);
      AddRecord("traceZonesDropped", fmt::format("count={}", m_droppedZones), false);
    }
  }


  void TraceLog::WriteSchemaIfNeeded()
  {
    if (m_schemaWritten)
    {
      return;
    }
    m_schemaWritten = true;
    // The values are part of the schema, so none can be added from now on
    m_table.LockValues();

    TraceSchema schema;
    schema.Values = m_table.GetValues();
    schema.Tracks = m_tracks;
    schema.Spans = m_spans;
    schema.Marks = m_marks;
    schema.Counters = m_counters;
    schema.HasFrameBounds = m_frameBegin.IsValid() && m_frameEnd.IsValid();
    if (schema.HasFrameBounds)
    {
      schema.FrameBeginIndex = m_frameBegin.Value - 1u;
      schema.FrameEndIndex = m_frameEnd.Value - 1u;
    }
    m_writer.SetSchema(std::move(schema));
  }


  void TraceLog::WriteRow(const TraceFrameRow& row)
  {
    WriteSchemaIfNeeded();
    m_writer.AddFrame(row);
  }
}
