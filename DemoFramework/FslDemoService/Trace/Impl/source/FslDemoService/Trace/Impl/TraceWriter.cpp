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
#include <FslDemoService/Trace/Impl/TraceWriter.hpp>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <system_error>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! How often the records are given to the sink
      constexpr std::chrono::milliseconds WriteInterval(250);
    }
  }


  bool TraceWriter::Batch::IsEmpty() const noexcept
  {
    return !HasThread && !HasProcessName && ZoneNames.empty() && !HasSchema && Events.empty() && Zones.empty() && Frames.empty();
  }


  void TraceWriter::Batch::Clear() noexcept
  {
    HasThread = false;
    HasProcessName = false;
    ZoneNames.clear();
    HasSchema = false;
    Events.clear();
    Zones.clear();
    Frames.clear();
  }


  TraceWriter::TraceWriter(std::unique_ptr<ITraceSink> sink, const bool useThread)
    : m_sink(std::move(sink))
  {
    if (!m_sink)
    {
      throw std::invalid_argument("sink can not be null");
    }
    if (useThread)
    {
      try
      {
        m_thread = std::thread([this]() { Run(); });
      }
      catch (const std::system_error& ex)
      {
        // A platform without threads: everything is written when the writer is closed
        FSLLOG3_VERBOSE("Trace: the trace is written when it is closed, as no thread could be started: {}", ex.what());
      }
    }
  }


  TraceWriter::~TraceWriter()
  {
    Close();
  }


  void TraceWriter::SetThread(const uint64_t threadId, const std::string_view name)
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_failed && !m_stop)
    {
      m_pending.HasThread = true;
      m_pending.ThreadId = threadId;
      m_pending.ThreadName = std::string(name);
    }
  }


  void TraceWriter::SetProcessName(const std::string_view name)
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_failed && !m_stop)
    {
      m_pending.HasProcessName = true;
      m_pending.ProcessName = std::string(name);
    }
  }


  void TraceWriter::AddZoneName(const uint32_t zone, const std::string_view name)
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_failed && !m_stop)
    {
      m_pending.ZoneNames.emplace_back(zone, std::string(name));
    }
  }


  void TraceWriter::SetSchema(TraceSchema schema)
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_failed && !m_stop)
    {
      m_pending.HasSchema = true;
      m_pending.Schema = std::move(schema);
    }
  }


  void TraceWriter::AddEvent(TraceEventRecord event)
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_failed && !m_stop)
    {
      m_pending.Events.push_back(std::move(event));
    }
  }


  void TraceWriter::AddZones(const std::span<const TraceZoneRecord> zones)
  {
    if (zones.empty())
    {
      return;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_failed && !m_stop)
    {
      m_pending.Zones.insert(m_pending.Zones.end(), zones.begin(), zones.end());
    }
  }


  void TraceWriter::AddFrame(const TraceFrameRow& row)
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_failed && !m_stop)
    {
      m_pending.Frames.push_back(row);
    }
  }


  void TraceWriter::Close() noexcept
  {
    if (m_isClosed)
    {
      return;
    }
    m_isClosed = true;
    {
      const std::lock_guard<std::mutex> lock(m_mutex);
      m_stop = true;
    }
    if (m_thread.joinable())
    {
      m_wake.notify_all();
      m_thread.join();
    }
    else
    {
      Batch scratch;
      WritePending(scratch);
    }
    try
    {
      m_sink->Close();
    }
    catch (const std::exception&)
    {
      const std::lock_guard<std::mutex> lock(m_mutex);
      m_failed = true;
    }
  }


  bool TraceWriter::HasFailed() const noexcept
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_failed;
  }


  void TraceWriter::Run() noexcept
  {
    // The batch is swapped with the waiting one, so its memory is used again
    Batch scratch;
    bool stop = false;
    while (!stop)
    {
      {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wake.wait_for(lock, LocalConfig::WriteInterval, [this]() { return m_stop; });
        stop = m_stop;
      }
      // What was added before the stop is written as well
      WritePending(scratch);
    }
  }


  void TraceWriter::WritePending(Batch& rScratch) noexcept
  {
    bool hasFailed = false;
    {
      const std::lock_guard<std::mutex> lock(m_mutex);
      hasFailed = m_failed;
      rScratch.Clear();
      std::swap(rScratch, m_pending);
    }
    if (hasFailed || rScratch.IsEmpty())
    {
      return;
    }
    try
    {
      // A name is written before what uses it, and the begin and the end of a zone are kept in their order
      if (rScratch.HasThread)
      {
        m_sink->WriteThread(rScratch.ThreadId, rScratch.ThreadName);
      }
      if (rScratch.HasProcessName)
      {
        m_sink->WriteProcessName(rScratch.ProcessName);
      }
      for (const auto& entry : rScratch.ZoneNames)
      {
        m_sink->WriteZoneName(entry.first, entry.second);
      }
      if (rScratch.HasSchema)
      {
        m_sink->WriteSchema(rScratch.Schema);
      }
      for (const TraceEventRecord& event : rScratch.Events)
      {
        m_sink->WriteEvent(event);
      }
      if (!rScratch.Zones.empty())
      {
        m_sink->WriteZones(rScratch.Zones);
      }
      for (const TraceFrameRow& row : rScratch.Frames)
      {
        m_sink->WriteFrame(row);
      }
      m_sink->Flush();
    }
    catch (const std::exception&)
    {
      // Nothing is logged from this thread, the owner asks HasFailed
      const std::lock_guard<std::mutex> lock(m_mutex);
      m_failed = true;
    }
  }
}
