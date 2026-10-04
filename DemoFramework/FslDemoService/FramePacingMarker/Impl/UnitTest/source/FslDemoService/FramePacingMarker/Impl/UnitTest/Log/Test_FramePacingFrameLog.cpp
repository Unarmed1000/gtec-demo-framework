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


#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingFrameLog.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingLogFileSink.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingLogWriter.hpp>
#include <algorithm>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

using namespace Fsl;

namespace
{
  using Test_FramePacingFrameLog = TestFixtureFslBase;
  using Test_FramePacingLogWriter = TestFixtureFslBase;
  using Test_FramePacingLogFileSink = TestFixtureFslBase;

  //! What was written, shared with the test as the writer owns the sink
  struct SinkContent
  {
    std::mutex Mutex;
    std::string Frames;
    std::string Events;
    uint32_t FlushCount{0};
    bool FailWrites{false};
  };

  class MemorySink final : public IFramePacingLogSink
  {
    std::shared_ptr<SinkContent> m_content;

  public:
    explicit MemorySink(std::shared_ptr<SinkContent> content)
      : m_content(std::move(content))
    {
    }

    void Write(const FramePacingLogStream stream, const std::string_view text) final
    {
      const std::lock_guard<std::mutex> lock(m_content->Mutex);
      if (m_content->FailWrites)
      {
        throw std::runtime_error("the disk is full");
      }
      (stream == FramePacingLogStream::Frames ? m_content->Frames : m_content->Events).append(text);
    }

    void Flush() final
    {
      const std::lock_guard<std::mutex> lock(m_content->Mutex);
      ++m_content->FlushCount;
    }
  };

  std::size_t CountLines(const std::string& text)
  {
    return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
  }
}


TEST(Test_FramePacingLogWriter, NullSink)
{
  EXPECT_THROW(FramePacingLogWriter(nullptr), std::invalid_argument);
}


TEST(Test_FramePacingLogWriter, Close_WritesWhatWasAppended_WithoutThread)
{
  const auto content = std::make_shared<SinkContent>();
  FramePacingLogWriter writer(std::make_unique<MemorySink>(content), false);
  EXPECT_FALSE(writer.IsThreaded());

  writer.Append(FramePacingLogStream::Frames, "a\n");
  writer.Append(FramePacingLogStream::Events, "e\n");
  writer.Append(FramePacingLogStream::Frames, "b\n");
  // Nothing is written before the writer is closed
  EXPECT_EQ("", content->Frames);

  writer.Close();

  EXPECT_EQ("a\nb\n", content->Frames);
  EXPECT_EQ("e\n", content->Events);
  EXPECT_EQ(1u, content->FlushCount);
  EXPECT_FALSE(writer.HasFailed());
}


TEST(Test_FramePacingLogWriter, Close_WritesWhatWasAppended_WithThread)
{
  const auto content = std::make_shared<SinkContent>();
  std::string expected;
  {
    FramePacingLogWriter writer(std::make_unique<MemorySink>(content));
    for (uint32_t i = 0; i < 2000; ++i)
    {
      const std::string line = std::to_string(i) + "\n";
      expected += line;
      writer.Append(FramePacingLogStream::Frames, line);
    }
    writer.Close();
    EXPECT_FALSE(writer.HasFailed());
  }

  // Everything, and in the order it was appended
  EXPECT_EQ(expected, content->Frames);
  EXPECT_GE(content->FlushCount, 1u);
}


TEST(Test_FramePacingLogWriter, Destructor_Closes)
{
  const auto content = std::make_shared<SinkContent>();
  {
    FramePacingLogWriter writer(std::make_unique<MemorySink>(content));
    writer.Append(FramePacingLogStream::Events, "e\n");
  }

  EXPECT_EQ("e\n", content->Events);
}


TEST(Test_FramePacingLogWriter, Append_AfterClose_IsIgnored)
{
  const auto content = std::make_shared<SinkContent>();
  FramePacingLogWriter writer(std::make_unique<MemorySink>(content), false);
  writer.Close();

  writer.Append(FramePacingLogStream::Frames, "late\n");
  writer.Close();

  EXPECT_EQ("", content->Frames);
}


TEST(Test_FramePacingLogWriter, FailingSink_IsReported)
{
  const auto content = std::make_shared<SinkContent>();
  content->FailWrites = true;
  FramePacingLogWriter writer(std::make_unique<MemorySink>(content), false);
  writer.Append(FramePacingLogStream::Frames, "a\n");

  writer.Close();

  EXPECT_TRUE(writer.HasFailed());
  EXPECT_EQ("", content->Frames);
}


TEST(Test_FramePacingFrameLog, EventsHaveAHeaderAndTheFormatVersion)
{
  const auto content = std::make_shared<SinkContent>();
  {
    const FramePacingFrameLog log(std::make_unique<MemorySink>(content), false);
  }

  EXPECT_EQ(0u, content->Events.find("frameIndex,timeTicks,event,details\n"));
  EXPECT_NE(std::string::npos, content->Events.find(",fact,formatVersion=1\n"));
  // A log without a frame still has the header of the frames
  EXPECT_EQ("frameIndex\n", content->Frames);
}


TEST(Test_FramePacingFrameLog, OneRowPerFrame_InOrder)
{
  const auto content = std::make_shared<SinkContent>();
  const uint64_t frames = (FramePacingFrameLogTable::OpenRowCount * 3u) + 5u;
  {
    FramePacingFrameLog log(std::make_unique<MemorySink>(content), false);
    const FramePacingLogColumn a = log.RegisterColumn("a", FramePacingLogUnit::Count, "the frame times ten");
    for (uint64_t frame = 0; frame < frames; ++frame)
    {
      log.BeginFrame(frame);
      EXPECT_EQ(frame, log.GetFrameIndex());
      log.SetInt64(a, static_cast<int64_t>(frame) * 10);
    }
  }

  std::string expected("frameIndex,a\n");
  for (uint64_t frame = 0; frame < frames; ++frame)
  {
    expected += std::to_string(frame) + "," + std::to_string(frame * 10u) + "\n";
  }
  EXPECT_EQ(expected, content->Frames);
}


TEST(Test_FramePacingFrameLog, ALateValueLandsInItsFrame)
{
  const auto content = std::make_shared<SinkContent>();
  {
    FramePacingFrameLog log(std::make_unique<MemorySink>(content), false);
    const FramePacingLogColumn shown = log.RegisterColumn("shownTicks", FramePacingLogUnit::Ticks, "When the frame was shown");
    for (uint64_t frame = 0; frame < 6; ++frame)
    {
      log.BeginFrame(frame);
      if (frame >= 3u)
      {
        // When a frame was shown is known three frames later
        log.SetInt64At(frame - 3u, shown, static_cast<int64_t>(1000 + frame));
      }
    }
  }

  EXPECT_EQ("frameIndex,shownTicks\n0,1003\n1,1004\n2,1005\n3,\n4,\n5,\n", content->Frames);
}


TEST(Test_FramePacingFrameLog, UnsignedValues)
{
  const auto content = std::make_shared<SinkContent>();
  {
    FramePacingFrameLog log(std::make_unique<MemorySink>(content), false);
    const FramePacingLogColumn raw = log.RegisterColumn("rawNanoseconds", FramePacingLogUnit::Nanoseconds, "");
    log.BeginFrame(0);
    log.SetUInt64(raw, 18446744073709551615ull);
    log.BeginFrame(1);
    log.SetUInt64At(0, raw, 1791046337983763328ull);
  }

  EXPECT_EQ("frameIndex,rawNanoseconds\n0,1791046337983763328\n1,\n", content->Frames);
}


TEST(Test_FramePacingFrameLog, ColumnsAreDescribedInTheEvents)
{
  const auto content = std::make_shared<SinkContent>();
  {
    FramePacingFrameLog log(std::make_unique<MemorySink>(content), false);
    log.RegisterColumn("presentCallTicks", FramePacingLogUnit::Ticks, "When present was called, by the host");
    log.BeginFrame(0);
  }

  // The description has a comma, so the field is quoted
  EXPECT_NE(std::string::npos,
            content->Events.find(",column,\"name=presentCallTicks;unit=ticks;description=When present was called, by the host\"\n"));
}


TEST(Test_FramePacingFrameLog, AColumnCanNotBeAddedAfterTheFirstRowsWereWritten)
{
  const auto content = std::make_shared<SinkContent>();
  {
    FramePacingFrameLog log(std::make_unique<MemorySink>(content), false);
    log.RegisterColumn("a", FramePacingLogUnit::Count, "");
    for (uint64_t frame = 0; frame <= FramePacingFrameLogTable::OpenRowCount; ++frame)
    {
      log.BeginFrame(frame);
    }

    // The first row was written, and with it the names of the columns
    EXPECT_FALSE(log.RegisterColumn("tooLate", FramePacingLogUnit::Count, "").IsValid());
  }

  EXPECT_EQ(0u, content->Frames.find("frameIndex,a\n"));
  EXPECT_EQ(FramePacingFrameLogTable::OpenRowCount + 2u, CountLines(content->Frames));
}


TEST(Test_FramePacingFrameLog, EventsCarryTheFrame)
{
  const auto content = std::make_shared<SinkContent>();
  {
    FramePacingFrameLog log(std::make_unique<MemorySink>(content), false);
    log.BeginFrame(41);
    log.AddEvent("swapchainCreated", "width=1600;height=900");
    log.AddFact("api", "Vulkan");
  }

  EXPECT_NE(std::string::npos, content->Events.find("\n41,"));
  EXPECT_NE(std::string::npos, content->Events.find(",swapchainCreated,width=1600;height=900\n"));
  EXPECT_NE(std::string::npos, content->Events.find(",fact,api=Vulkan\n"));
}


TEST(Test_FramePacingFrameLog, NothingIsAddedAfterClose)
{
  const auto content = std::make_shared<SinkContent>();
  FramePacingFrameLog log(std::make_unique<MemorySink>(content), false);
  log.BeginFrame(0);
  log.Close();
  const std::string frames = content->Frames;
  const std::string events = content->Events;

  log.BeginFrame(1);
  log.AddEvent("late", "");
  log.Close();

  EXPECT_EQ(frames, content->Frames);
  EXPECT_EQ(events, content->Events);
}


TEST(Test_FramePacingLogFileSink, ToEventsPath)
{
  EXPECT_EQ(IO::Path("frames.events.csv"), FramePacingLogFileSink::ToEventsPath(IO::Path("frames.csv")));
  EXPECT_EQ(IO::Path("C:/logs/run.a/frames.events.csv"), FramePacingLogFileSink::ToEventsPath(IO::Path("C:/logs/run.a/frames.csv")));
  // No extension: the dot of a directory is not the one of the file
  EXPECT_EQ(IO::Path("C:/logs/run.a/frames.events.csv"), FramePacingLogFileSink::ToEventsPath(IO::Path("C:/logs/run.a/frames")));
  EXPECT_EQ(IO::Path("frames.events.csv"), FramePacingLogFileSink::ToEventsPath(IO::Path("frames")));
  EXPECT_EQ(IO::Path("a.b.events.csv"), FramePacingLogFileSink::ToEventsPath(IO::Path("a.b.csv")));
}
