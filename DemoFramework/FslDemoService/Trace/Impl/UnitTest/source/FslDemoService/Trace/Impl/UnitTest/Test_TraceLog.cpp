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
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDemoService/Trace/Impl/TraceLog.hpp>
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace Fsl;

namespace
{
  using Test_TraceLog = TestFixtureFslBase;

  //! What a sink was given, in the order it was given
  struct Recorded
  {
    std::vector<std::string> Calls;
    std::vector<std::pair<uint32_t, std::string>> ZoneNames;
    std::vector<TraceSchema> Schemas;
    std::vector<TraceEventRecord> Events;
    std::vector<TraceZoneRecord> Zones;
    std::vector<TraceFrameRow> Frames;
    uint64_t ThreadId{0};
    std::string ThreadName;
    std::string ProcessName;
    bool IsClosed{false};

    [[nodiscard]] const TraceEventRecord* TryFind(const std::string_view name) const
    {
      const auto itrFind = std::find_if(Events.begin(), Events.end(), [name](const TraceEventRecord& entry) { return entry.Name == name; });
      return itrFind != Events.end() ? &(*itrFind) : nullptr;
    }
  };

  class RecordingSink final : public ITraceSink
  {
    std::shared_ptr<Recorded> m_recorded;

  public:
    explicit RecordingSink(std::shared_ptr<Recorded> recorded)
      : m_recorded(std::move(recorded))
    {
    }

    void WriteThread(const uint64_t threadId, const std::string_view name) final
    {
      m_recorded->Calls.emplace_back("thread");
      m_recorded->ThreadId = threadId;
      m_recorded->ThreadName = std::string(name);
    }

    void WriteProcessName(const std::string_view name) final
    {
      m_recorded->Calls.emplace_back("process");
      m_recorded->ProcessName = std::string(name);
    }

    void WriteZoneName(const uint32_t zone, const std::string_view name) final
    {
      m_recorded->Calls.emplace_back("zoneName");
      m_recorded->ZoneNames.emplace_back(zone, std::string(name));
    }

    void WriteSchema(const TraceSchema& schema) final
    {
      m_recorded->Calls.emplace_back("schema");
      m_recorded->Schemas.push_back(schema);
    }

    void WriteEvent(const TraceEventRecord& event) final
    {
      m_recorded->Calls.emplace_back("event");
      m_recorded->Events.push_back(event);
    }

    void WriteZones(const std::span<const TraceZoneRecord> zones) final
    {
      m_recorded->Calls.emplace_back("zones");
      m_recorded->Zones.insert(m_recorded->Zones.end(), zones.begin(), zones.end());
    }

    void WriteFrame(const TraceFrameRow& row) final
    {
      m_recorded->Calls.emplace_back("frame");
      m_recorded->Frames.push_back(row);
    }

    void Flush() final
    {
    }

    void Close() final
    {
      m_recorded->IsClosed = true;
    }
  };

  //! A trace that gives everything to the sink when it is closed, so a test sees all of it after Close
  std::unique_ptr<TraceLog> CreateLog(const std::shared_ptr<Recorded>& recorded, const bool anonymise = true)
  {
    return std::make_unique<TraceLog>(std::make_unique<RecordingSink>(recorded), anonymise, false);
  }
}


TEST(Test_TraceLog, AValueOfAEarlierFrameLandsInThatFrame)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue value = log->RegisterValue("displayTicks", TraceUnit::Ticks, "When the frame was shown");

  log->BeginFrame(0, 5);
  log->BeginFrame(1, 5);
  log->BeginFrame(2, 6);
  log->SetValueAt(2, value, TickCount(2000));
  // Known two frames later
  log->SetValueAt(0, value, TickCount(1000));
  log->Close();

  ASSERT_EQ(3u, recorded->Frames.size());
  EXPECT_EQ(0u, recorded->Frames[0].FrameIndex);
  EXPECT_EQ(5u, recorded->Frames[0].RunId);
  EXPECT_TRUE(recorded->Frames[0].HasValue[0]);
  EXPECT_EQ(1000, recorded->Frames[0].Values[0]);
  EXPECT_FALSE(recorded->Frames[1].HasValue[0]);
  EXPECT_EQ(6u, recorded->Frames[2].RunId);
  EXPECT_EQ(2000, recorded->Frames[2].Values[0]);
  EXPECT_TRUE(recorded->IsClosed);
}


TEST(Test_TraceLog, SetValueAt_ATimeIsWrittenInTheUnitOfTheValue)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue ticks = log->RegisterValue("timeTicks", TraceUnit::Ticks, "");
  const TraceValue nanoseconds = log->RegisterValue("timeNs", TraceUnit::NanosecondTicks, "");

  log->BeginFrame(0, 1);
  log->SetValueAt(0, ticks, TickCount(1234));
  log->SetValueAt(0, nanoseconds, TickCount(1234));
  log->BeginFrame(1, 1);
  // The tick the moment lies in
  log->SetValueAt(1, ticks, NanosecondTickCount(123499));
  log->SetValueAt(1, nanoseconds, NanosecondTickCount(123499));
  log->BeginFrame(2, 1);
  log->SetValueAt(2, ticks, NanosecondTickCount(-1));
  log->Close();

  ASSERT_EQ(3u, recorded->Frames.size());
  EXPECT_EQ(1234, recorded->Frames[0].Values[ticks.Value - 1u]);
  EXPECT_EQ(123400, recorded->Frames[0].Values[nanoseconds.Value - 1u]);
  EXPECT_EQ(1234, recorded->Frames[1].Values[ticks.Value - 1u]);
  EXPECT_EQ(123499, recorded->Frames[1].Values[nanoseconds.Value - 1u]);
  EXPECT_EQ(-1, recorded->Frames[2].Values[ticks.Value - 1u]);
}


TEST(Test_TraceLog, SetValueAt_ADurationIsWrittenInTheUnitOfTheValue)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue ticks = log->RegisterValue("durationTicks", TraceUnit::DurationTicks, "");
  const TraceValue nanoseconds = log->RegisterValue("durationNs", TraceUnit::Nanoseconds, "");

  log->BeginFrame(0, 1);
  log->SetValueAt(0, ticks, TimeSpan(41664));
  log->SetValueAt(0, nanoseconds, TimeSpan(41664));
  log->BeginFrame(1, 1);
  // Rounded to the nearest tick
  log->SetValueAt(1, ticks, NanosecondTimeSpan(4166389));
  log->SetValueAt(1, nanoseconds, NanosecondTimeSpan(4166389));
  log->BeginFrame(2, 1);
  log->SetValueAt(2, ticks, NanosecondTimeSpan(4166349));
  log->Close();

  ASSERT_EQ(3u, recorded->Frames.size());
  EXPECT_EQ(41664, recorded->Frames[0].Values[ticks.Value - 1u]);
  EXPECT_EQ(4166400, recorded->Frames[0].Values[nanoseconds.Value - 1u]);
  EXPECT_EQ(41664, recorded->Frames[1].Values[ticks.Value - 1u]);
  EXPECT_EQ(4166389, recorded->Frames[1].Values[nanoseconds.Value - 1u]);
  EXPECT_EQ(41663, recorded->Frames[2].Values[ticks.Value - 1u]);
}


TEST(Test_TraceLog, SetValueAt_NotWrittenToAValueOfAnotherUnit)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue time = log->RegisterValue("timeTicks", TraceUnit::Ticks, "");
  const TraceValue timeNs = log->RegisterValue("timeNs", TraceUnit::NanosecondTicks, "");
  const TraceValue duration = log->RegisterValue("durationTicks", TraceUnit::DurationTicks, "");
  const TraceValue durationNs = log->RegisterValue("durationNs", TraceUnit::Nanoseconds, "");
  const TraceValue count = log->RegisterValue("count", TraceUnit::Count, "");

  log->BeginFrame(0, 1);
  // A duration is not a time
  log->SetValueAt(0, time, TimeSpan(1));
  log->SetValueAt(0, timeNs, NanosecondTimeSpan(1));
  // A time is not a duration
  log->SetValueAt(0, duration, TickCount(1));
  log->SetValueAt(0, durationNs, NanosecondTickCount(1));
  // A count is neither
  log->SetValueAt(0, count, TickCount(1));
  log->SetValueAt(0, count, NanosecondTimeSpan(1));
  log->SetFlagAt(0, count, true);
  // Not a value
  log->SetValueAt(0, TraceValue(), TickCount(1));
  log->SetFlagAt(0, TraceValue(), true);
  log->Close();

  ASSERT_EQ(1u, recorded->Frames.size());
  for (const TraceValue value : {time, timeNs, duration, durationNs, count})
  {
    EXPECT_FALSE(recorded->Frames[0].HasValue[value.Value - 1u]);
  }
}


TEST(Test_TraceLog, SetNumbers_EachIsWrittenToAValueOfItsUnit)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue count = log->RegisterValue("count", TraceUnit::Count, "");
  const TraceValue id = log->RegisterValue("id", TraceUnit::Id, "");
  const TraceValue code = log->RegisterValue("code", TraceUnit::Code, "");
  const TraceValue pixels = log->RegisterValue("pixels", TraceUnit::Pixels, "");
  const TraceValue otherClock = log->RegisterValue("driverNs", TraceUnit::Nanoseconds, "");

  log->BeginFrame(0, 1);
  log->BeginFrame(1, 1);
  log->SetCountAt(1, count, 7);
  log->SetIdAt(1, id, 42);
  log->SetCodeAt(1, code, -3);
  log->SetPixelsAt(1, pixels, PxValue(4));
  log->SetRawNanosecondsAt(1, otherClock, 123456789012345);
  // Of a earlier frame
  log->SetCountAt(0, count, 6);
  log->Close();

  ASSERT_EQ(2u, recorded->Frames.size());
  EXPECT_EQ(6, recorded->Frames[0].Values[count.Value - 1u]);
  EXPECT_FALSE(recorded->Frames[0].HasValue[id.Value - 1u]);
  EXPECT_EQ(7, recorded->Frames[1].Values[count.Value - 1u]);
  EXPECT_EQ(42, recorded->Frames[1].Values[id.Value - 1u]);
  EXPECT_EQ(-3, recorded->Frames[1].Values[code.Value - 1u]);
  EXPECT_EQ(4, recorded->Frames[1].Values[pixels.Value - 1u]);
  EXPECT_EQ(123456789012345, recorded->Frames[1].Values[otherClock.Value - 1u]);
}


TEST(Test_TraceLog, SetNumbers_NotWrittenToAValueOfAnotherUnit)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue count = log->RegisterValue("count", TraceUnit::Count, "");
  const TraceValue id = log->RegisterValue("id", TraceUnit::Id, "");
  const TraceValue code = log->RegisterValue("code", TraceUnit::Code, "");
  const TraceValue pixels = log->RegisterValue("pixels", TraceUnit::Pixels, "");
  const TraceValue otherClock = log->RegisterValue("driverNs", TraceUnit::Nanoseconds, "");
  const TraceValue time = log->RegisterValue("timeTicks", TraceUnit::Ticks, "");
  const TraceValue flag = log->RegisterValue("flag", TraceUnit::Flag, "");

  log->BeginFrame(0, 1);
  // Each to the value of the next one
  log->SetCountAt(0, id, 1);
  log->SetIdAt(0, code, 1);
  log->SetCodeAt(0, pixels, 1);
  log->SetPixelsAt(0, otherClock, PxValue(1));
  log->SetRawNanosecondsAt(0, count, 1);
  // A number is no time and no flag
  log->SetCountAt(0, time, 1);
  log->SetIdAt(0, time, 1);
  log->SetCodeAt(0, flag, 1);
  // Not a value
  log->SetCountAt(0, TraceValue(), 1);
  log->Close();

  ASSERT_EQ(1u, recorded->Frames.size());
  for (const TraceValue value : {count, id, code, pixels, otherClock, time, flag})
  {
    EXPECT_FALSE(recorded->Frames[0].HasValue[value.Value - 1u]);
  }
}


TEST(Test_TraceLog, SetFlagAt_AFlagIsOneOrZero)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue flag = log->RegisterValue("flag", TraceUnit::Flag, "");

  log->BeginFrame(0, 1);
  log->SetFlagAt(0, flag, true);
  log->BeginFrame(1, 1);
  log->SetFlagAt(1, flag, false);
  log->Close();

  ASSERT_EQ(2u, recorded->Frames.size());
  EXPECT_TRUE(recorded->Frames[0].HasValue[flag.Value - 1u]);
  EXPECT_EQ(1, recorded->Frames[0].Values[flag.Value - 1u]);
  EXPECT_TRUE(recorded->Frames[1].HasValue[flag.Value - 1u]);
  EXPECT_EQ(0, recorded->Frames[1].Values[flag.Value - 1u]);
}


TEST(Test_TraceLog, TheSchemaIsWrittenOnceBeforeTheFirstFrame)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue begin = log->RegisterValue("beginTicks", TraceUnit::Ticks, "");
  const TraceValue end = log->RegisterValue("endTicks", TraceUnit::Ticks, "");
  const TraceTrack track = log->RegisterTrack("Calls", TraceTrackKind::Sequential);
  ASSERT_TRUE(log->DeclareSpan("call", track, begin, end, TraceLink::FrameChain));
  log->SetFrameBounds(begin, end);

  log->BeginFrame(0, 1);
  log->BeginFrame(1, 1);
  log->Close();

  ASSERT_EQ(1u, recorded->Schemas.size());
  const TraceSchema& schema = recorded->Schemas[0];
  ASSERT_EQ(2u, schema.Values.size());
  ASSERT_EQ(1u, schema.Tracks.size());
  EXPECT_EQ("Calls", schema.Tracks[0].Name);
  ASSERT_EQ(1u, schema.Spans.size());
  EXPECT_EQ("call", schema.Spans[0].Title);
  EXPECT_EQ(0u, schema.Spans[0].BeginIndex);
  EXPECT_EQ(1u, schema.Spans[0].EndIndex);
  EXPECT_EQ(TraceLink::FrameChain, schema.Spans[0].Link);
  EXPECT_TRUE(schema.HasFrameBounds);

  const auto itrSchema = std::find(recorded->Calls.begin(), recorded->Calls.end(), "schema");
  const auto itrFrame = std::find(recorded->Calls.begin(), recorded->Calls.end(), "frame");
  ASSERT_NE(recorded->Calls.end(), itrSchema);
  ASSERT_NE(recorded->Calls.end(), itrFrame);
  EXPECT_LT(itrSchema, itrFrame);
}


TEST(Test_TraceLog, ATraceWithoutAFrameStillHasItsSchema)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  log->RegisterValue("a", TraceUnit::Count, "");
  log->Close();

  ASSERT_EQ(1u, recorded->Schemas.size());
  EXPECT_EQ(1u, recorded->Schemas[0].Values.size());
  EXPECT_TRUE(recorded->Frames.empty());
}


TEST(Test_TraceLog, Declare_OnlyTimesAreSpansAndMarks)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue time = log->RegisterValue("timeTicks", TraceUnit::Ticks, "");
  const TraceValue count = log->RegisterValue("count", TraceUnit::Count, "");
  const TraceTrack track = log->RegisterTrack("Track", TraceTrackKind::Lanes);

  EXPECT_FALSE(log->DeclareSpan("span", track, time, count, TraceLink::NoLink));
  EXPECT_FALSE(log->DeclareSpan("span", track, count, time, TraceLink::NoLink));
  EXPECT_FALSE(log->DeclareSpan("span", TraceTrack(), time, time, TraceLink::NoLink));
  EXPECT_FALSE(log->DeclareSpan("span", track, time, TraceValue(), TraceLink::NoLink));
  EXPECT_FALSE(log->DeclareMark("mark", track, count, TraceLink::NoLink));
  // A moment is not something to draw a graph of
  EXPECT_FALSE(log->DeclareCounter("counter", time));

  EXPECT_TRUE(log->DeclareSpan("span", track, time, time, TraceLink::NoLink));
  EXPECT_TRUE(log->DeclareMark("mark", track, time, TraceLink::NoLink));
  EXPECT_TRUE(log->DeclareCounter("counter", count));
  log->Close();

  ASSERT_EQ(1u, recorded->Schemas.size());
  EXPECT_EQ(1u, recorded->Schemas[0].Spans.size());
  EXPECT_EQ(1u, recorded->Schemas[0].Marks.size());
  EXPECT_EQ(1u, recorded->Schemas[0].Counters.size());
}


TEST(Test_TraceLog, Declare_AMomentInNanosecondsIsAMark)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue time = log->RegisterValue("timeTicks", TraceUnit::Ticks, "");
  const TraceValue moment = log->RegisterValue("momentNs", TraceUnit::NanosecondTicks, "");
  const TraceValue duration = log->RegisterValue("durationNs", TraceUnit::Nanoseconds, "");
  const TraceTrack track = log->RegisterTrack("Track", TraceTrackKind::Lanes);

  EXPECT_FALSE(log->DeclareSpan("span", track, time, moment, TraceLink::NoLink));
  EXPECT_FALSE(log->DeclareSpan("span", track, moment, moment, TraceLink::NoLink));
  EXPECT_FALSE(log->DeclareMark("mark", track, duration, TraceLink::NoLink));
  // A moment is not something to draw a graph of
  EXPECT_FALSE(log->DeclareCounter("counter", moment));

  EXPECT_TRUE(log->DeclareMark("mark", track, moment, TraceLink::NoLink));
  EXPECT_TRUE(log->DeclareCounter("counter", duration));
  log->Close();

  ASSERT_EQ(1u, recorded->Schemas.size());
  EXPECT_EQ(0u, recorded->Schemas[0].Spans.size());
  ASSERT_EQ(1u, recorded->Schemas[0].Marks.size());
  EXPECT_EQ(moment.Value - 1u, recorded->Schemas[0].Marks[0].TimeIndex);
  EXPECT_EQ(1u, recorded->Schemas[0].Counters.size());
}


TEST(Test_TraceLog, Declare_TooLateOnceAFrameWasWritten)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceValue time = log->RegisterValue("timeTicks", TraceUnit::Ticks, "");
  const TraceTrack track = log->RegisterTrack("Track", TraceTrackKind::Sequential);

  // The first frame is written when its place is needed
  for (uint32_t i = 0; i <= TraceFrameTable::OpenRowCount; ++i)
  {
    log->BeginFrame(i, 1);
  }
  EXPECT_FALSE(log->DeclareMark("mark", track, time, TraceLink::NoLink));
  EXPECT_FALSE(log->RegisterValue("late", TraceUnit::Count, "").IsValid());
  EXPECT_FALSE(log->RegisterTrack("Late", TraceTrackKind::Sequential).IsValid());
  // A track that exists is still found
  EXPECT_EQ(track, log->RegisterTrack("Track", TraceTrackKind::Sequential));
}


TEST(Test_TraceLog, Zones_AreKeptInTheOrderTheyHappened)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceZone outer = log->RegisterZone("Draw");
  const TraceZone inner = log->RegisterZone("Present");
  EXPECT_EQ(outer, log->RegisterZone("Draw"));

  log->BeginZoneAt(outer, TickCount(100));
  log->BeginZoneAt(inner, TickCount(110));
  log->EndZoneAt(TickCount(120));
  log->EndZoneAt(TickCount(130));
  log->Close();

  ASSERT_EQ(2u, recorded->ZoneNames.size());
  EXPECT_EQ(1u, recorded->ZoneNames[0].first);
  EXPECT_EQ("Draw", recorded->ZoneNames[0].second);
  ASSERT_EQ(4u, recorded->Zones.size());
  EXPECT_TRUE(recorded->Zones[0].IsBegin);
  EXPECT_EQ(outer.Value, recorded->Zones[0].Zone);
  EXPECT_EQ(100, recorded->Zones[0].Ticks);
  EXPECT_TRUE(recorded->Zones[1].IsBegin);
  EXPECT_EQ(inner.Value, recorded->Zones[1].Zone);
  EXPECT_FALSE(recorded->Zones[2].IsBegin);
  EXPECT_EQ(120, recorded->Zones[2].Ticks);
  EXPECT_FALSE(recorded->Zones[3].IsBegin);
  EXPECT_EQ(130, recorded->Zones[3].Ticks);

  // The name of a zone is given to the sink before the zones
  const auto itrName = std::find(recorded->Calls.begin(), recorded->Calls.end(), "zoneName");
  const auto itrZones = std::find(recorded->Calls.begin(), recorded->Calls.end(), "zones");
  ASSERT_NE(recorded->Calls.end(), itrZones);
  EXPECT_LT(itrName, itrZones);
}


TEST(Test_TraceLog, Zones_ATimeThatGoesBackIsHeldAtTheLastOne)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceZone zone = log->RegisterZone("Zone");

  log->BeginZoneAt(zone, TickCount(200));
  log->EndZoneAt(TickCount(150));
  log->Close();

  ASSERT_EQ(2u, recorded->Zones.size());
  EXPECT_EQ(200, recorded->Zones[0].Ticks);
  EXPECT_EQ(200, recorded->Zones[1].Ticks);
}


TEST(Test_TraceLog, Zones_AZoneThatIsNotAZoneTakesItsEndWithIt)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceZone zone = log->RegisterZone("Zone");

  log->BeginZoneAt(zone, TickCount(10));
  // Not a zone: its end must not end the zone around it
  log->BeginZoneAt(TraceZone(), TickCount(20));
  log->EndZoneAt(TickCount(30));
  log->EndZoneAt(TickCount(40));
  log->Close();

  ASSERT_EQ(2u, recorded->Zones.size());
  EXPECT_TRUE(recorded->Zones[0].IsBegin);
  EXPECT_FALSE(recorded->Zones[1].IsBegin);
  EXPECT_EQ(40, recorded->Zones[1].Ticks);
  EXPECT_EQ(1u, log->GetDroppedZones());
}


TEST(Test_TraceLog, Zones_AEndWithoutABeginIsIgnored)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);

  log->EndZoneAt(TickCount(10));
  log->Close();

  EXPECT_TRUE(recorded->Zones.empty());
}


TEST(Test_TraceLog, Zones_AZoneThatIsOpenEndsWhenTheTraceIsClosed)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceZone zone = log->RegisterZone("Zone");

  log->BeginZoneAt(zone, TickCount(10));
  log->BeginZoneAt(zone, TickCount(20));
  log->Close();

  ASSERT_EQ(4u, recorded->Zones.size());
  EXPECT_TRUE(recorded->Zones[0].IsBegin);
  EXPECT_TRUE(recorded->Zones[1].IsBegin);
  EXPECT_FALSE(recorded->Zones[2].IsBegin);
  EXPECT_FALSE(recorded->Zones[3].IsBegin);
}


TEST(Test_TraceLog, Zones_WhenTooManyWaitWholeZonesAreDropped)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceZone zone = log->RegisterZone("Zone");

  // Zones that begin and do not end: they are not handed over, so they fill what can wait
  const std::size_t beginCount = TraceLog::MaxWaitingZoneRecords + 100u;
  for (std::size_t i = 0; i < beginCount; ++i)
  {
    log->BeginZoneAt(zone, TickCount(static_cast<int64_t>(i)));
  }
  for (std::size_t i = 0; i < beginCount; ++i)
  {
    log->EndZoneAt(TickCount(static_cast<int64_t>(beginCount + i)));
  }
  log->Close();

  EXPECT_EQ(100u, log->GetDroppedZones());
  // Every begin that was kept has its end, and no end comes before its begin
  std::size_t begins = 0;
  std::size_t ends = 0;
  for (const TraceZoneRecord& record : recorded->Zones)
  {
    if (record.IsBegin)
    {
      ++begins;
    }
    else
    {
      ++ends;
      EXPECT_LE(ends, begins);
    }
  }
  EXPECT_EQ(TraceLog::MaxWaitingZoneRecords, begins);
  EXPECT_EQ(begins, ends);
  // The trace says itself that it lacks zones
  const TraceEventRecord* const pEvent = recorded->TryFind("traceZonesDropped");
  ASSERT_NE(nullptr, pEvent);
  EXPECT_EQ("count=100", pEvent->Details);
}


TEST(Test_TraceLog, Events_CarryTheFrameTheyWereWrittenIn)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);

  log->AddEvent("early", "a=1");
  log->BeginFrame(41, 3);
  log->AddEvent("swapchainCreated", "width=1920;height=1080");
  log->SetFact("api", "Vulkan");
  log->Close();

  ASSERT_EQ(3u, recorded->Events.size());
  EXPECT_FALSE(recorded->Events[0].HasFrame);
  EXPECT_FALSE(recorded->Events[0].IsFact);
  EXPECT_TRUE(recorded->Events[1].HasFrame);
  EXPECT_EQ(41u, recorded->Events[1].FrameIndex);
  EXPECT_EQ(3u, recorded->Events[1].RunId);
  EXPECT_EQ("swapchainCreated", recorded->Events[1].Name);
  EXPECT_EQ("width=1920;height=1080", recorded->Events[1].Details);
  EXPECT_TRUE(recorded->Events[2].IsFact);
  EXPECT_EQ("api", recorded->Events[2].Name);
  EXPECT_EQ("Vulkan", recorded->Events[2].Details);
}


TEST(Test_TraceLog, Anonymise_TheHardwareIsNotNamed)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded, true);
  log->AddAnonymousText("Example Device 1000", "Example GPU");
  log->AddAnonymousFact("vulkan.deviceId", "0x0");
  log->AddAnonymousDirectory(R"(c:\Users\somebody)", "<home>");

  log->SetFact("vulkan.deviceName", "Example Device 1000");
  log->SetFact("vulkan.deviceId", "0x1234");
  log->SetFact("vulkan.vendorId", "0x1234");
  log->AddEvent("device", "name=Example Device 1000;path=C:/Users/Somebody/app");
  log->Close();

  ASSERT_EQ(4u, recorded->Events.size());
  EXPECT_EQ("Example GPU", recorded->Events[0].Details);
  EXPECT_EQ("0x0", recorded->Events[1].Details);
  // Only the fact that was named is replaced as a whole
  EXPECT_EQ("0x1234", recorded->Events[2].Details);
  EXPECT_EQ("name=Example GPU;path=<home>/app", recorded->Events[3].Details);
}


TEST(Test_TraceLog, Anonymise_OffKeepsTheNames)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded, false);
  log->AddAnonymousText("Example Device 1000", "Example GPU");
  log->AddAnonymousFact("vulkan.deviceId", "0x0");

  log->SetFact("vulkan.deviceName", "Example Device 1000");
  log->SetFact("vulkan.deviceId", "0x1234");
  log->Close();

  ASSERT_EQ(2u, recorded->Events.size());
  EXPECT_EQ("Example Device 1000", recorded->Events[0].Details);
  EXPECT_EQ("0x1234", recorded->Events[1].Details);
}


TEST(Test_TraceLog, TheThreadAndTheProcessAreGivenToTheSink)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);

  log->SetThread(1234, "Main");
  log->SetProcessName("Example.App");
  log->Close();

  EXPECT_EQ(1234u, recorded->ThreadId);
  EXPECT_EQ("Main", recorded->ThreadName);
  EXPECT_EQ("Example.App", recorded->ProcessName);
}


TEST(Test_TraceLog, NothingIsRecordedAfterClose)
{
  const auto recorded = std::make_shared<Recorded>();
  const auto log = CreateLog(recorded);
  const TraceZone zone = log->RegisterZone("Zone");
  log->Close();

  log->BeginZoneAt(zone, TickCount(10));
  log->EndZoneAt(TickCount(20));
  log->AddEvent("late", "");
  log->BeginFrame(0, 1);

  EXPECT_TRUE(recorded->Zones.empty());
  EXPECT_TRUE(recorded->Events.empty());
  EXPECT_TRUE(recorded->Frames.empty());
}
