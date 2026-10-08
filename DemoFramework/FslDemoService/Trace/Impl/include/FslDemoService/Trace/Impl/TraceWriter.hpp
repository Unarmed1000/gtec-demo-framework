#ifndef FSLDEMOSERVICE_TRACE_IMPL_TRACEWRITER_HPP
#define FSLDEMOSERVICE_TRACE_IMPL_TRACEWRITER_HPP
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


#include <FslDemoService/Trace/Impl/ITraceSink.hpp>
#include <FslDemoService/Trace/Impl/TraceRecords.hpp>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace Fsl
{
  //! Hands the records of the trace to the sink on a thread of its own, so the thread that records never waits for the sink.
  //!
  //! What is added is only kept, a thread gives it to the sink four times a second. Without a thread everything is given to the sink
  //! when the writer is closed.
  class TraceWriter final
  {
    //! What waits for the sink
    struct Batch
    {
      bool HasThread{false};
      uint64_t ThreadId{0};
      std::string ThreadName;
      bool HasProcessName{false};
      std::string ProcessName;
      std::vector<std::pair<uint32_t, std::string>> ZoneNames;
      bool HasSchema{false};
      TraceSchema Schema;
      std::vector<TraceEventRecord> Events;
      std::vector<TraceZoneRecord> Zones;
      std::vector<TraceFrameRow> Frames;

      [[nodiscard]] bool IsEmpty() const noexcept;
      void Clear() noexcept;
    };

    std::unique_ptr<ITraceSink> m_sink;
    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    Batch m_pending;
    bool m_stop{false};
    bool m_failed{false};
    bool m_isClosed{false};
    std::thread m_thread;

  public:
    TraceWriter(const TraceWriter&) = delete;
    TraceWriter& operator=(const TraceWriter&) = delete;

    //! @param sink where the records go (required).
    //! @param useThread false to give everything to the sink when the writer is closed.
    explicit TraceWriter(std::unique_ptr<ITraceSink> sink, const bool useThread = true);
    ~TraceWriter();

    void SetThread(const uint64_t threadId, const std::string_view name);
    void SetProcessName(const std::string_view name);
    void AddZoneName(const uint32_t zone, const std::string_view name);
    void SetSchema(TraceSchema schema);
    void AddEvent(TraceEventRecord event);
    void AddZones(const std::span<const TraceZoneRecord> zones);
    void AddFrame(const TraceFrameRow& row);

    //! @brief Give what waits to the sink, close it and stop the thread.
    void Close() noexcept;

    //! @return true if the sink failed, what was added after that is dropped.
    [[nodiscard]] bool HasFailed() const noexcept;

  private:
    void Run() noexcept;
    void WritePending(Batch& rScratch) noexcept;
  };
}

#endif
