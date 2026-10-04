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
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingLogWriter.hpp>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! How often the text is written and flushed
      constexpr std::chrono::milliseconds WriteInterval(250);
    }

    constexpr std::size_t ToIndex(const FramePacingLogStream stream) noexcept
    {
      return stream == FramePacingLogStream::Frames ? 0u : 1u;
    }

    constexpr FramePacingLogStream ToStream(const std::size_t index) noexcept
    {
      return index == 0u ? FramePacingLogStream::Frames : FramePacingLogStream::Events;
    }
  }


  FramePacingLogWriter::FramePacingLogWriter(std::unique_ptr<IFramePacingLogSink> sink, const bool useThread)
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
        FSLLOG3_VERBOSE("FramePacing: the log is written when it is closed, as no thread could be started: {}", ex.what());
      }
    }
  }


  FramePacingLogWriter::~FramePacingLogWriter()
  {
    Close();
  }


  void FramePacingLogWriter::Append(const FramePacingLogStream stream, const std::string_view text)
  {
    if (text.empty())
    {
      return;
    }
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_failed && !m_stop)
    {
      m_pending[ToIndex(stream)].append(text);
    }
  }


  void FramePacingLogWriter::Close() noexcept
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
      std::array<std::string, StreamCount> scratch;
      WritePending(scratch);
    }
  }


  bool FramePacingLogWriter::HasFailed() const noexcept
  {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_failed;
  }


  void FramePacingLogWriter::Run() noexcept
  {
    // The buffers are swapped with the waiting ones, so their memory is used again
    std::array<std::string, StreamCount> scratch;
    bool stop = false;
    while (!stop)
    {
      {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wake.wait_for(lock, LocalConfig::WriteInterval, [this]() { return m_stop; });
        stop = m_stop;
      }
      // What was appended before the stop is written as well
      WritePending(scratch);
    }
  }


  void FramePacingLogWriter::WritePending(std::array<std::string, StreamCount>& rScratch) noexcept
  {
    bool hasFailed = false;
    {
      const std::lock_guard<std::mutex> lock(m_mutex);
      hasFailed = m_failed;
      for (std::size_t i = 0; i < StreamCount; ++i)
      {
        rScratch[i].clear();
        rScratch[i].swap(m_pending[i]);
      }
    }
    if (hasFailed)
    {
      return;
    }
    try
    {
      bool hasText = false;
      for (std::size_t i = 0; i < StreamCount; ++i)
      {
        if (!rScratch[i].empty())
        {
          m_sink->Write(ToStream(i), rScratch[i]);
          hasText = true;
        }
      }
      if (hasText)
      {
        m_sink->Flush();
      }
    }
    catch (const std::exception&)
    {
      // Nothing is logged from this thread, the owner asks HasFailed
      const std::lock_guard<std::mutex> lock(m_mutex);
      m_failed = true;
    }
  }
}
