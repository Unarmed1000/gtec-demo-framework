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
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslDemoService/Trace/Perfetto/PerfettoTraceSink.hpp>
#include <fcntl.h>
#include <perfetto.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <share.h>
#else
#include <unistd.h>
#endif

// This is the only file that includes the Perfetto SDK. What the trace holds (the names of its tracks, events and arguments, its clock
// and how another tool adds to it) is described in Doc/Trace.md: a change here that changes one of them is a change of the format.

PERFETTO_DEFINE_CATEGORIES(perfetto::Category("fsl").SetDescription("The trace service of the demo framework"));
PERFETTO_TRACK_EVENT_STATIC_STORAGE();

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr uint32_t BufferSizeKb = 32 * 1024;
      constexpr uint32_t SharedMemorySizeKb = 4096;
      constexpr uint32_t SharedMemoryPageSizeKb = 32;
      constexpr uint32_t FileWritePeriodMs = 1000;
      constexpr uint32_t FlushTimeoutMs = 5000;
      //! The lanes a track is drawn in at most
      constexpr std::size_t MaxLanes = 16;

      constexpr std::string_view TrackFrames("Frames");
      constexpr std::string_view TrackEvents("Events");
      constexpr std::string_view TrackFacts("Facts");
      constexpr std::string_view TrackSchema("Schema");

      constexpr int32_t RankFrames = 0;
      constexpr int32_t RankFirstTrack = 10;
      constexpr int32_t RankEvents = 1000;
      constexpr int32_t RankFacts = 1001;
      constexpr int32_t RankSchema = 1002;
    }

    //! A time of the framework clock (100 nanosecond ticks) as a time of the trace. The framework clock is the monotonic clock of the
    //! system, and every time is stamped with it, so the times of the trace are those of the framework to the tick.
    perfetto::TraceTimestamp ToTimestamp(const int64_t ticks) noexcept
    {
      return {static_cast<uint32_t>(perfetto::protos::gen::BUILTIN_CLOCK_MONOTONIC), static_cast<uint64_t>(std::max<int64_t>(ticks, 0)) * 100u};
    }

    //! FNV-1a, so the id of a track only depends on its name
    constexpr uint64_t Hash(const std::string_view text, uint64_t hash = 14695981039346656037ull) noexcept
    {
      for (const char ch : text)
      {
        hash ^= static_cast<uint8_t>(ch);
        hash *= 1099511628211ull;
      }
      return hash;
    }

    uint64_t ToTrackId(const std::string_view name, const std::size_t lane) noexcept
    {
      uint64_t hash = Hash("fsl.trace.track/");
      hash = Hash(name, hash);
      hash ^= static_cast<uint64_t>(lane) + 1u;
      hash *= 1099511628211ull;
      return hash != 0u ? hash : 1u;
    }

    perfetto::Track ToTrack(const uint64_t trackId)
    {
      // A track of the process, which is what a track is when it is given no parent
      return perfetto::Track(trackId);
    }

    //! The id that links the steps of a frame, see Doc/Trace.md
    constexpr uint64_t ToFlowId(const uint32_t runId, const uint64_t frameIndex) noexcept
    {
      return (static_cast<uint64_t>(runId) << 32u) | (frameIndex & 0xFFFFFFFFu);
    }

    const char* ToString(const TraceUnit unit) noexcept
    {
      switch (unit)
      {
      case TraceUnit::Ticks:
        return "ticks";
      case TraceUnit::DurationTicks:
        return "durationTicks";
      case TraceUnit::Nanoseconds:
        return "nanoseconds";
      case TraceUnit::Count:
        return "count";
      case TraceUnit::Id:
        return "id";
      case TraceUnit::Flag:
        return "flag";
      case TraceUnit::Pixels:
        return "pixels";
      case TraceUnit::Code:
        return "code";
      }
      return "unknown";
    }

    void DefineTrack(const uint64_t trackId, const std::string& name, const int32_t rank)
    {
      const perfetto::Track track = ToTrack(trackId);
      auto descriptor = track.Serialize();
      descriptor.set_name(name);
      descriptor.set_sibling_order_rank(rank);
      perfetto::TrackEvent::SetTrackDescriptor(track, descriptor);
    }

    //! "key=value;key=value" as arguments of a event
    void AddDetails(perfetto::EventContext& rContext, const std::string_view details)
    {
      std::size_t begin = 0;
      while (begin < details.size())
      {
        std::size_t end = details.find(';', begin);
        if (end == std::string_view::npos)
        {
          end = details.size();
        }
        const std::string_view entry = details.substr(begin, end - begin);
        const std::size_t separator = entry.find('=');
        if (separator != std::string_view::npos && separator > 0u)
        {
          const std::string_view key = entry.substr(0, separator);
          rContext.AddDebugAnnotation(perfetto::DynamicString(key.data(), key.size()), std::string(entry.substr(separator + 1u)));
        }
        begin = end + 1u;
      }
    }

    void InitializePerfetto()
    {
      static std::once_flag g_once;
      std::call_once(g_once,
                     []()
                     {
                       perfetto::TracingInitArgs args;
                       args.backends = perfetto::kInProcessBackend;
                       args.shmem_size_hint_kb = LocalConfig::SharedMemorySizeKb;
                       args.shmem_page_size_hint_kb = LocalConfig::SharedMemoryPageSizeKb;
                       // Everything is written by the writer thread of the trace service, which can wait: nothing is to be dropped
                       args.track_event_buffer_exhausted_policy = perfetto::BufferExhaustedPolicy::kStallThenDrop;
                       perfetto::Tracing::Initialize(args);
                       perfetto::TrackEvent::Register();
                     });
    }

    int OpenFile(const IO::Path& path)
    {
      const std::string& utf8Path = path.ToUTF8String();
      const std::filesystem::path nativePath(std::u8string_view(reinterpret_cast<const char8_t*>(utf8Path.data()), utf8Path.size()));
#ifdef _WIN32
      int fd = -1;
      // Binary: the trace is not text
      if (_wsopen_s(&fd, nativePath.c_str(), _O_RDWR | _O_CREAT | _O_TRUNC | _O_BINARY, _SH_DENYWR, _S_IREAD | _S_IWRITE) != 0)
      {
        return -1;
      }
      return fd;
#else
      return ::open(nativePath.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
#endif
    }

    void CloseFile(const int fd) noexcept
    {
#ifdef _WIN32
      _close(fd);
#else
      ::close(fd);
#endif
    }


    class PerfettoSink final : public ITraceSink
    {
      //! A row of a track that holds spans that do not overlap, or are inside each other
      struct Lane
      {
        uint64_t TrackId{0};
        //! When the spans that are open on the lane end, the innermost last
        std::vector<int64_t> OpenEnds;
      };

      struct TrackState
      {
        std::string Name;
        TraceTrackKind Kind{TraceTrackKind::Sequential};
        int32_t Rank{0};
        std::vector<Lane> Lanes;
      };

      struct PendingSpan
      {
        const TraceSpanInfo* Info{nullptr};
        int64_t BeginTicks{0};
        int64_t EndTicks{0};
      };

      std::unique_ptr<perfetto::TracingSession> m_session;
      int m_fd;
      bool m_anonymise;
      bool m_isClosed{false};
      int64_t m_startTicks;

      bool m_hasThread{false};
      uint64_t m_threadId{0};
      uint64_t m_fallbackThreadTrackId;

      //! A deque, so the name of a zone stays where it is: the trace is given the pointer to it
      std::deque<std::string> m_zoneNames;

      bool m_hasSchema{false};
      TraceSchema m_schema;
      std::vector<TrackState> m_tracks;
      TrackState m_frames;
      uint64_t m_eventsTrackId;
      uint64_t m_factsTrackId;
      uint64_t m_schemaTrackId;
      std::vector<PendingSpan> m_pendingSpans;
      int64_t m_lastFrameTicks;
      uint64_t m_overflowSpans{0};

    public:
      PerfettoSink(std::unique_ptr<perfetto::TracingSession> session, const int fd, const bool anonymise)
        : m_session(std::move(session))
        , m_fd(fd)
        , m_anonymise(anonymise)
        , m_startTicks(HighResolutionTimer().GetTimestamp().Ticks())
        , m_fallbackThreadTrackId(ToTrackId("Main thread", 0))
        , m_eventsTrackId(ToTrackId(LocalConfig::TrackEvents, 0))
        , m_factsTrackId(ToTrackId(LocalConfig::TrackFacts, 0))
        , m_schemaTrackId(ToTrackId(LocalConfig::TrackSchema, 0))
        , m_lastFrameTicks(m_startTicks)
      {
        m_frames.Name = std::string(LocalConfig::TrackFrames);
        m_frames.Kind = TraceTrackKind::Sequential;
        m_frames.Rank = LocalConfig::RankFrames;
        DefineTrack(m_eventsTrackId, std::string(LocalConfig::TrackEvents), LocalConfig::RankEvents);
        DefineTrack(m_factsTrackId, std::string(LocalConfig::TrackFacts), LocalConfig::RankFacts);
        DefineTrack(m_schemaTrackId, std::string(LocalConfig::TrackSchema), LocalConfig::RankSchema);
        DefineTrack(m_fallbackThreadTrackId, "Main thread", LocalConfig::RankFrames);
        WriteProcess({});
      }

      PerfettoSink(const PerfettoSink&) = delete;
      PerfettoSink& operator=(const PerfettoSink&) = delete;
      PerfettoSink(PerfettoSink&&) = delete;
      PerfettoSink& operator=(PerfettoSink&&) = delete;

      ~PerfettoSink() final
      {
        try
        {
          Close();
        }
        catch (const std::exception& ex)
        {
          // A destructor does not throw
          FSLLOG3_ERROR("Trace: closing the trace failed: {}", ex.what());
        }
      }


      void WriteThread(const uint64_t threadId, const std::string_view name) final
      {
        m_hasThread = true;
        m_threadId = threadId;
        // The descriptor is written here, by name: left to the SDK the track gets the name of the thread that writes
        const perfetto::ThreadTrack track = GetThreadTrack();
        auto descriptor = track.Serialize();
        descriptor.mutable_thread()->set_thread_name(std::string(name));
        perfetto::TrackEvent::SetTrackDescriptor(track, descriptor);
      }


      void WriteProcessName(const std::string_view name) final
      {
        WriteProcess(name);
      }


      void WriteZoneName(const uint32_t zone, const std::string_view name) final
      {
        // The zones are numbered from one in the order they were added
        if (zone == (m_zoneNames.size() + 1u))
        {
          m_zoneNames.emplace_back(name);
        }
      }


      void WriteSchema(const TraceSchema& schema) final
      {
        if (m_hasSchema)
        {
          return;
        }
        m_hasSchema = true;
        m_schema = schema;
        m_tracks.resize(m_schema.Tracks.size());
        for (std::size_t i = 0; i < m_tracks.size(); ++i)
        {
          m_tracks[i].Name = m_schema.Tracks[i].Name;
          m_tracks[i].Kind = m_schema.Tracks[i].Kind;
          m_tracks[i].Rank = LocalConfig::RankFirstTrack + static_cast<int32_t>(i);
        }

        // What the frames hold is in the trace itself, so a reader does not need the source that wrote it
        const perfetto::Track track = ToTrack(m_schemaTrackId);
        const perfetto::TraceTimestamp time = ToTimestamp(m_startTicks);
        for (const TraceValueInfo& value : m_schema.Values)
        {
          TRACE_EVENT_INSTANT("fsl", "value", track, time, "name", value.Name, "unit", ToString(value.Unit), "description", value.Description);
        }
        for (const TraceTrackInfo& info : m_schema.Tracks)
        {
          TRACE_EVENT_INSTANT("fsl", "track", track, time, "name", info.Name, "kind", info.Kind == TraceTrackKind::Lanes ? "lanes" : "sequential");
        }
        for (const TraceSpanInfo& span : m_schema.Spans)
        {
          TRACE_EVENT_INSTANT("fsl", "span", track, time, "title", span.Title, "track", m_schema.Tracks[span.TrackIndex].Name, "begin",
                              m_schema.Values[span.BeginIndex].Name, "end", m_schema.Values[span.EndIndex].Name, "frameChain",
                              span.Link == TraceLink::FrameChain);
        }
        for (const TraceMarkInfo& mark : m_schema.Marks)
        {
          TRACE_EVENT_INSTANT("fsl", "mark", track, time, "title", mark.Title, "track", m_schema.Tracks[mark.TrackIndex].Name, "time",
                              m_schema.Values[mark.TimeIndex].Name, "frameChain", mark.Link == TraceLink::FrameChain);
        }
        for (const TraceCounterInfo& counter : m_schema.Counters)
        {
          TRACE_EVENT_INSTANT("fsl", "counter", track, time, "title", counter.Title, "value", m_schema.Values[counter.ValueIndex].Name);
        }
        if (m_schema.HasFrameBounds)
        {
          TRACE_EVENT_INSTANT("fsl", "frameBounds", track, time, "begin", m_schema.Values[m_schema.FrameBeginIndex].Name, "end",
                              m_schema.Values[m_schema.FrameEndIndex].Name);
        }
      }


      void WriteEvent(const TraceEventRecord& event) final
      {
        const perfetto::TraceTimestamp time = ToTimestamp(event.Ticks);
        if (event.IsFact)
        {
          TRACE_EVENT_INSTANT("fsl", perfetto::DynamicString(event.Name), ToTrack(m_factsTrackId), time, "value", event.Details);
          return;
        }
        TRACE_EVENT_INSTANT("fsl", perfetto::DynamicString(event.Name), ToTrack(m_eventsTrackId), time,
                            [&event](perfetto::EventContext context)
                            {
                              if (event.HasFrame)
                              {
                                context.AddDebugAnnotation("frameIndex", static_cast<int64_t>(event.FrameIndex));
                                context.AddDebugAnnotation("runId", static_cast<int64_t>(event.RunId));
                              }
                              context.AddDebugAnnotation("details", event.Details);
                              AddDetails(context, event.Details);
                            });
      }


      void WriteZones(const std::span<const TraceZoneRecord> zones) final
      {
        if (m_hasThread)
        {
          WriteZonesTo(GetThreadTrack(), zones);
        }
        else
        {
          WriteZonesTo(ToTrack(m_fallbackThreadTrackId), zones);
        }
      }


      void WriteFrame(const TraceFrameRow& row) final
      {
        if (!m_hasSchema)
        {
          return;
        }
        const auto frameIndex = static_cast<int64_t>(row.FrameIndex);
        const auto runId = static_cast<int64_t>(row.RunId);
        const uint64_t flowId = ToFlowId(row.RunId, row.FrameIndex);

        // The frame itself: a span from where it began to where it ended, with every value it has as the number it was set to
        int64_t frameBeginTicks = m_lastFrameTicks;
        int64_t frameEndTicks = m_lastFrameTicks;
        if (m_schema.HasFrameBounds && row.HasValue[m_schema.FrameBeginIndex])
        {
          frameBeginTicks = row.Values[m_schema.FrameBeginIndex];
          frameEndTicks = row.HasValue[m_schema.FrameEndIndex] ? std::max(row.Values[m_schema.FrameEndIndex], frameBeginTicks) : frameBeginTicks;
        }
        m_lastFrameTicks = frameBeginTicks;
        {
          bool overflow = false;
          const perfetto::Track track = ToTrack(PlaceSpan(m_frames, frameBeginTicks, frameEndTicks, overflow));
          TRACE_EVENT_BEGIN("fsl", "Frame", track, ToTimestamp(frameBeginTicks), perfetto::Flow::Global(flowId),
                            [this, &row, frameIndex, runId](perfetto::EventContext context)
                            {
                              context.AddDebugAnnotation("frameIndex", frameIndex);
                              context.AddDebugAnnotation("runId", runId);
                              for (std::size_t i = 0; i < m_schema.Values.size(); ++i)
                              {
                                if (row.HasValue[i])
                                {
                                  // The names stay where they are for as long as the sink lives
                                  context.AddDebugAnnotation(m_schema.Values[i].Name.c_str(), row.Values[i]);
                                }
                              }
                            });
          TRACE_EVENT_END("fsl", track, ToTimestamp(frameEndTicks));
        }

        WriteSpans(row, frameIndex, runId, flowId);

        for (const TraceMarkInfo& mark : m_schema.Marks)
        {
          if (!row.HasValue[mark.TimeIndex])
          {
            continue;
          }
          const perfetto::Track track = ToTrack(GetFirstLaneTrackId(m_tracks[mark.TrackIndex]));
          const perfetto::TraceTimestamp time = ToTimestamp(row.Values[mark.TimeIndex]);
          if (mark.Link == TraceLink::FrameChain)
          {
            TRACE_EVENT_INSTANT("fsl", perfetto::StaticString(mark.Title.c_str()), track, time, perfetto::Flow::Global(flowId), "frameIndex",
                                frameIndex, "runId", runId);
          }
          else
          {
            TRACE_EVENT_INSTANT("fsl", perfetto::StaticString(mark.Title.c_str()), track, time, "frameIndex", frameIndex, "runId", runId);
          }
        }

        // A counter is a graph for the eye: its values are numbers with decimals in a trace, the exact ones are those of the frame
        for (const TraceCounterInfo& counter : m_schema.Counters)
        {
          if (!row.HasValue[counter.ValueIndex])
          {
            continue;
          }
          const int64_t value = row.Values[counter.ValueIndex];
          if (m_schema.Values[counter.ValueIndex].Unit == TraceUnit::DurationTicks)
          {
            const auto track = perfetto::CounterTrack(perfetto::DynamicString(counter.Title)).set_unit(perfetto::CounterTrack::Unit::UNIT_TIME_NS);
            TRACE_COUNTER("fsl", track, ToTimestamp(frameBeginTicks), value * 100);
          }
          else
          {
            TRACE_COUNTER("fsl", perfetto::CounterTrack(perfetto::DynamicString(counter.Title)), ToTimestamp(frameBeginTicks), value);
          }
        }
      }


      void Flush() final
      {
        if (!m_isClosed)
        {
          perfetto::TrackEvent::Flush();
        }
      }


      void Close() final
      {
        if (m_isClosed)
        {
          return;
        }
        m_isClosed = true;
        FSLLOG3_VERBOSE_IF(m_overflowSpans > 0u, "Trace: {} spans did not fit the track they were declared on and were drawn below it",
                           m_overflowSpans);
        perfetto::TrackEvent::Flush();
        m_session->FlushBlocking(LocalConfig::FlushTimeoutMs);
        m_session->StopBlocking();
        m_session.reset();
        CloseFile(m_fd);
        m_fd = -1;
      }

    private:
      [[nodiscard]] perfetto::ThreadTrack GetThreadTrack() const
      {
        return perfetto::ThreadTrack::ForThread(static_cast<perfetto::base::PlatformThreadId>(m_threadId));
      }


      void WriteProcess(const std::string_view name)
      {
        const perfetto::ProcessTrack track = perfetto::ProcessTrack::Current();
        auto descriptor = track.Serialize();
        // The tracks are drawn in the order of their ranks
        descriptor.set_child_ordering(perfetto::protos::gen::TrackDescriptor::EXPLICIT);
        if (!name.empty())
        {
          descriptor.mutable_process()->set_process_name(std::string(name));
        }
        if (m_anonymise)
        {
          // The command line of the process names the directories of the machine
          descriptor.mutable_process()->clear_cmdline();
          if (!name.empty())
          {
            descriptor.mutable_process()->add_cmdline(std::string(name));
          }
        }
        perfetto::TrackEvent::SetTrackDescriptor(track, descriptor);
      }


      template <typename TTrack>
      void WriteZonesTo(const TTrack& track, const std::span<const TraceZoneRecord> zones)
      {
        for (const TraceZoneRecord& record : zones)
        {
          if (!record.IsBegin)
          {
            TRACE_EVENT_END("fsl", track, ToTimestamp(record.Ticks));
          }
          else if (record.Zone >= 1u && record.Zone <= m_zoneNames.size())
          {
            TRACE_EVENT_BEGIN("fsl", perfetto::StaticString(m_zoneNames[record.Zone - 1u].c_str()), track, ToTimestamp(record.Ticks));
          }
          else
          {
            // A begin without a name still has its end, so it is written to keep the two in step
            TRACE_EVENT_BEGIN("fsl", "zone", track, ToTimestamp(record.Ticks));
          }
        }
      }


      void WriteSpans(const TraceFrameRow& row, const int64_t frameIndex, const int64_t runId, const uint64_t flowId)
      {
        m_pendingSpans.clear();
        for (const TraceSpanInfo& span : m_schema.Spans)
        {
          if (row.HasValue[span.BeginIndex] && row.HasValue[span.EndIndex] && row.Values[span.EndIndex] >= row.Values[span.BeginIndex])
          {
            m_pendingSpans.push_back(PendingSpan{&span, row.Values[span.BeginIndex], row.Values[span.EndIndex]});
          }
        }
        // In the order they begin, and a span before the ones inside it: that is the order a track takes them in
        std::stable_sort(m_pendingSpans.begin(), m_pendingSpans.end(), [](const PendingSpan& lhs, const PendingSpan& rhs)
                         { return lhs.BeginTicks != rhs.BeginTicks ? lhs.BeginTicks < rhs.BeginTicks : lhs.EndTicks > rhs.EndTicks; });
        for (const PendingSpan& span : m_pendingSpans)
        {
          bool overflow = false;
          const perfetto::Track track = ToTrack(PlaceSpan(m_tracks[span.Info->TrackIndex], span.BeginTicks, span.EndTicks, overflow));
          const bool isLinked = span.Info->Link == TraceLink::FrameChain;
          TRACE_EVENT_BEGIN("fsl", perfetto::StaticString(span.Info->Title.c_str()), track, ToTimestamp(span.BeginTicks),
                            [frameIndex, runId, flowId, overflow, isLinked](perfetto::EventContext context)
                            {
                              if (isLinked)
                              {
                                perfetto::Flow::Global(flowId)(context);
                              }
                              context.AddDebugAnnotation("frameIndex", frameIndex);
                              context.AddDebugAnnotation("runId", runId);
                              if (overflow)
                              {
                                context.AddDebugAnnotation("overflow", true);
                              }
                            });
          TRACE_EVENT_END("fsl", track, ToTimestamp(span.EndTicks));
        }
      }


      //! @brief Find the lane of a track a span fits on: one where it does not cut through a span that is open.
      //! @param rOverflow true if the span is on a lane it was not meant for: it overlaps a span of a track whose spans were declared
      //!        not to overlap, or the track has no lane left.
      //! @return the id of the track of the lane.
      uint64_t PlaceSpan(TrackState& rTrack, const int64_t beginTicks, const int64_t endTicks, bool& rOverflow)
      {
        rOverflow = false;
        for (std::size_t laneIndex = 0; laneIndex < rTrack.Lanes.size(); ++laneIndex)
        {
          std::vector<int64_t>& rOpenEnds = rTrack.Lanes[laneIndex].OpenEnds;
          while (!rOpenEnds.empty() && rOpenEnds.back() <= beginTicks)
          {
            rOpenEnds.pop_back();
          }
          // A span of a sequential track can be inside the one that is open, the spans of lanes each have a lane to themselves
          if (rOpenEnds.empty() || (rTrack.Kind == TraceTrackKind::Sequential && endTicks <= rOpenEnds.back()))
          {
            rOpenEnds.push_back(endTicks);
            rOverflow = rTrack.Kind == TraceTrackKind::Sequential && laneIndex > 0u;
            m_overflowSpans += rOverflow ? 1u : 0u;
            return rTrack.Lanes[laneIndex].TrackId;
          }
        }
        if (rTrack.Lanes.size() < LocalConfig::MaxLanes)
        {
          Lane& rLane = AddLane(rTrack);
          rLane.OpenEnds.push_back(endTicks);
          rOverflow = rTrack.Kind == TraceTrackKind::Sequential && rTrack.Lanes.size() > 1u;
          m_overflowSpans += rOverflow ? 1u : 0u;
          return rLane.TrackId;
        }
        // No lane is left: the last one takes it, what was open there is drawn cut short
        Lane& rLane = rTrack.Lanes.back();
        rLane.OpenEnds.clear();
        rLane.OpenEnds.push_back(endTicks);
        rOverflow = true;
        ++m_overflowSpans;
        return rLane.TrackId;
      }


      Lane& AddLane(TrackState& rTrack)
      {
        Lane lane;
        lane.TrackId = ToTrackId(rTrack.Name, rTrack.Lanes.size());
        // The lanes of a track have its name, and a viewer draws tracks of one name as one
        DefineTrack(lane.TrackId, rTrack.Name, rTrack.Rank);
        rTrack.Lanes.push_back(std::move(lane));
        return rTrack.Lanes.back();
      }


      uint64_t GetFirstLaneTrackId(TrackState& rTrack)
      {
        return rTrack.Lanes.empty() ? AddLane(rTrack).TrackId : rTrack.Lanes.front().TrackId;
      }
    };
  }


  std::unique_ptr<ITraceSink> PerfettoTraceSink::TryCreate(const TraceSinkConfig& config)
  {
    int fd = -1;
    try
    {
      fd = OpenFile(config.Path);
      if (fd < 0)
      {
        FSLLOG3_ERROR("Trace: the trace is not written, the file '{}' could not be created", config.Path);
        return nullptr;
      }
      InitializePerfetto();

      perfetto::TraceConfig traceConfig;
      traceConfig.add_buffers()->set_size_kb(LocalConfig::BufferSizeKb);
      {
        auto* pDataSourceConfig = traceConfig.add_data_sources()->mutable_config();
        pDataSourceConfig->set_name("track_event");
        perfetto::protos::gen::TrackEventConfig trackEventConfig;
        trackEventConfig.add_enabled_categories("fsl");
        pDataSourceConfig->set_track_event_config_raw(trackEventConfig.SerializeAsString());
      }
      traceConfig.set_write_into_file(true);
      traceConfig.set_file_write_period_ms(LocalConfig::FileWritePeriodMs);
      {
        auto* pBuiltin = traceConfig.mutable_builtin_data_sources();
        // The times of the trace are those of the framework clock
        pBuiltin->set_primary_trace_clock(perfetto::protos::gen::BUILTIN_CLOCK_MONOTONIC);
        // A frame is written long after it happened. Without the markers of the service the trace is sorted as a whole when it is
        // read, with them what is late can be dropped. It is also what lets another tool add to the file.
        pBuiltin->set_disable_service_events(true);
      }

      auto session = perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);
      session->Setup(traceConfig, fd);
      session->StartBlocking();
      return std::make_unique<PerfettoSink>(std::move(session), fd, config.Anonymise);
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_ERROR("Trace: the trace is not written: {}", ex.what());
      if (fd >= 0)
      {
        CloseFile(fd);
      }
      return nullptr;
    }
  }
}
