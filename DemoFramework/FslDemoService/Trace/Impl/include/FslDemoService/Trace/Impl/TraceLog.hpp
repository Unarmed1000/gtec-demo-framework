#ifndef FSLDEMOSERVICE_TRACE_IMPL_TRACELOG_HPP
#define FSLDEMOSERVICE_TRACE_IMPL_TRACELOG_HPP
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


#include <FslBase/Math/Pixel/PxValue.hpp>
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslBase/Time/NanosecondTickCount.hpp>
#include <FslBase/Time/NanosecondTimeSpan.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoService/Trace/Impl/ITraceSink.hpp>
#include <FslDemoService/Trace/Impl/TraceAnonymiser.hpp>
#include <FslDemoService/Trace/Impl/TraceFrameTable.hpp>
#include <FslDemoService/Trace/Impl/TraceRecords.hpp>
#include <FslDemoService/Trace/Impl/TraceWriter.hpp>
#include <FslDemoService/Trace/TraceTypes.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Fsl
{
  //! The trace: the names that were registered, the rows of the frames that can still change, the zones that wait to be handed over and
  //! the writer. This is what the trace service uses to be a ITraceService. It is used from one thread.
  class TraceLog final
  {
  public:
    //! The begins and ends of zones that are kept before they are handed to the writer. They are handed over at every frame and when
    //! this many wait.
    static constexpr std::size_t ZoneHandOverCount = 2048;
    //! The most that wait. A zone that begins while this many wait is dropped, with the zones inside it.
    static constexpr std::size_t MaxWaitingZoneRecords = 16384;

  private:
    HighResolutionTimer m_timer;
    TraceFrameTable m_table;
    TraceWriter m_writer;
    TraceAnonymiser m_anonymiser;
    bool m_anonymise;
    //! The facts whose value is replaced as a whole while the trace is anonymised: the key and what is written for it
    std::vector<std::pair<std::string, std::string>> m_anonymousFacts;

    std::vector<std::string> m_zoneNames;
    std::vector<TraceTrackInfo> m_tracks;
    std::vector<TraceSpanInfo> m_spans;
    std::vector<TraceMarkInfo> m_marks;
    std::vector<TraceCounterInfo> m_counters;
    TraceValue m_frameBegin;
    TraceValue m_frameEnd;

    //! The zones that wait: a buffer of a fixed size (m_zoneCount of it is in use), so recording a zone never allocates
    std::vector<TraceZoneRecord> m_zones;
    std::size_t m_zoneCount{0};
    //! The zones that began, were kept and did not end yet
    uint32_t m_zoneDepth{0};
    //! The zones that began, were dropped and did not end yet
    uint32_t m_droppedZoneDepth{0};
    uint64_t m_droppedZones{0};
    uint64_t m_reportedDroppedZones{0};
    int64_t m_lastZoneTicks{0};

    TraceFrameRow m_closedRow;
    uint64_t m_frameIndex{0};
    uint32_t m_runId{0};
    bool m_hasFrame{false};
    bool m_schemaWritten{false};
    bool m_isClosed{false};
    bool m_handOverFailed{false};
    //! The values that were given a time or a duration they have no unit for, so it is said once for each
    std::array<bool, TraceFrameRow::MaxValues> m_wrongUnitReported{};

  public:
    //! @param sink where the records go (required).
    //! @param anonymise true to replace the texts and directories that were added in every event and fact.
    //! @param useThread false to give everything to the sink when the trace is closed.
    TraceLog(std::unique_ptr<ITraceSink> sink, const bool anonymise, const bool useThread = true);
    ~TraceLog();

    [[nodiscard]] bool IsAnonymised() const noexcept
    {
      return m_anonymise;
    }

    [[nodiscard]] uint64_t GetFrameIndex() const noexcept
    {
      return m_frameIndex;
    }

    [[nodiscard]] uint64_t GetDroppedZones() const noexcept
    {
      return m_droppedZones;
    }

    //! @brief The thread the zones are on.
    void SetThread(const uint64_t threadId, const std::string_view name);
    void SetProcessName(const std::string_view name);

    // Zones
    TraceZone RegisterZone(const std::string_view name);
    void BeginZone(const TraceZone zone) noexcept
    {
      BeginZoneAt(zone, m_timer.GetTimestamp());
    }
    void EndZone() noexcept
    {
      EndZoneAt(m_timer.GetTimestamp());
    }
    void BeginZoneAt(const TraceZone zone, const TickCount time) noexcept;
    void EndZoneAt(const TickCount time) noexcept;

    // Frame values
    TraceTrack RegisterTrack(const std::string_view name, const TraceTrackKind kind);
    TraceValue RegisterValue(const std::string_view name, const TraceUnit unit, const std::string_view description);
    [[nodiscard]] TraceValue FindValue(const std::string_view name) const noexcept
    {
      return m_table.FindValue(name);
    }
    bool DeclareSpan(const std::string_view title, const TraceTrack track, const TraceValue begin, const TraceValue end, const TraceLink link);
    bool DeclareMark(const std::string_view title, const TraceTrack track, const TraceValue time, const TraceLink link);
    bool DeclareCounter(const std::string_view title, const TraceValue value);
    void SetFrameBounds(const TraceValue begin, const TraceValue end);

    //! @brief A frame begins: its row is opened and the row that is too old to change is written.
    void BeginFrame(const uint64_t frameIndex, const uint32_t runId);

    //! @brief Set a count, a id, a code, pixels or a moment on a clock that is not the framework's (in the nanoseconds of that
    //!        clock): a value of the unit TraceUnit::Count, Id, Code, Pixels or Nanoseconds. It is not written to a value of another
    //!        unit, which is logged once.
    void SetCountAt(const uint64_t frameIndex, const TraceValue value, const uint64_t count) noexcept;
    void SetIdAt(const uint64_t frameIndex, const TraceValue value, const uint64_t id) noexcept;
    void SetCodeAt(const uint64_t frameIndex, const TraceValue value, const int64_t code) noexcept;
    void SetPixelsAt(const uint64_t frameIndex, const TraceValue value, const PxValue pixels) noexcept;
    void SetRawNanosecondsAt(const uint64_t frameIndex, const TraceValue value, const uint64_t nanoseconds) noexcept;

    //! @brief Set a moment. It is written in the unit of the value: TraceUnit::Ticks (the tick the moment lies in) or
    //!        TraceUnit::NanosecondTicks. It is not written to a value of another unit, which is logged once.
    void SetValueAt(const uint64_t frameIndex, const TraceValue value, const TickCount time) noexcept;
    void SetValueAt(const uint64_t frameIndex, const TraceValue value, const NanosecondTickCount time) noexcept;

    //! @brief Set a duration. It is written in the unit of the value: TraceUnit::DurationTicks (rounded to the nearest tick) or
    //!        TraceUnit::Nanoseconds. It is not written to a value of another unit, which is logged once.
    void SetValueAt(const uint64_t frameIndex, const TraceValue value, const TimeSpan duration) noexcept;
    void SetValueAt(const uint64_t frameIndex, const TraceValue value, const NanosecondTimeSpan duration) noexcept;

    //! @brief Set a flag: a value of the unit TraceUnit::Flag. It is not written to a value of another unit, which is logged once.
    void SetFlagAt(const uint64_t frameIndex, const TraceValue value, const bool flag) noexcept;

    // Events and facts
    void AddEvent(const std::string_view name, const std::string_view details);
    void SetFact(const std::string_view key, const std::string_view value);
    void AddAnonymousText(const std::string_view text, const std::string_view replacement);
    void AddAnonymousDirectory(const std::string_view directory, const std::string_view replacement);
    void AddAnonymousFact(const std::string_view key, const std::string_view replacement);

    //! @brief Write the rows that are still open and stop.
    void Close() noexcept;

    //! @return true if writing failed, the trace is then incomplete.
    [[nodiscard]] bool HasFailed() const noexcept
    {
      return m_writer.HasFailed();
    }

  private:
    [[nodiscard]] bool IsTrack(const TraceTrack track) const noexcept
    {
      return track.IsValid() && track.Value <= m_tracks.size();
    }

    void AddRecord(const std::string_view name, const std::string_view details, const bool isFact);
    void SetNumberAt(const uint64_t frameIndex, const TraceValue value, const TraceUnit unit, const int64_t number,
                     const std::string_view given) noexcept;
    void ReportWrongUnit(const TraceValue value, const std::string_view given) noexcept;
    void HandOverZones();
    void TryHandOverZones() noexcept;
    void WriteSchemaIfNeeded();
    void WriteRow(const TraceFrameRow& row);
  };
}

#endif
