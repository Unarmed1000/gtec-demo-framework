#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_FRAMEPACINGLOGWRITER_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_FRAMEPACINGLOGWRITER_HPP
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


#include <FslDemoService/FramePacingMarker/Impl/Log/IFramePacingLogSink.hpp>
#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace Fsl
{
  //! Writes the text of the frame pacing log on a thread of its own, so the thread that draws the frames never waits for a file.
  //!
  //! Append only adds the text to a buffer. The thread takes the buffers a few times a second, writes them and flushes, so a process that
  //! is killed loses a fraction of a second. If the thread can not be started the text is kept and written when the writer is closed.
  class FramePacingLogWriter final
  {
    static constexpr std::size_t StreamCount = 2;

    std::unique_ptr<IFramePacingLogSink> m_sink;
    mutable std::mutex m_mutex;
    std::condition_variable m_wake;
    //! Guarded by m_mutex: the text that is waiting to be written
    std::array<std::string, StreamCount> m_pending;
    //! Guarded by m_mutex
    bool m_stop{false};
    //! Guarded by m_mutex: the sink failed, nothing more is written
    bool m_failed{false};
    bool m_isClosed{false};
    //! The last member, so everything it uses exists when it starts
    std::thread m_thread;

  public:
    FramePacingLogWriter(const FramePacingLogWriter&) = delete;
    FramePacingLogWriter& operator=(const FramePacingLogWriter&) = delete;

    //! @param sink where the text goes (required).
    //! @param useThread false to write everything when the writer is closed (for tests, and what is done if the thread can not start).
    explicit FramePacingLogWriter(std::unique_ptr<IFramePacingLogSink> sink, const bool useThread = true);
    ~FramePacingLogWriter();

    //! @brief Add text to a stream. It does not touch a file.
    void Append(const FramePacingLogStream stream, const std::string_view text);

    //! @brief Write everything that is waiting and stop. Nothing can be appended after this.
    void Close() noexcept;

    //! @return true if writing failed, the log is then incomplete.
    [[nodiscard]] bool HasFailed() const noexcept;

    //! @return true if a thread does the writing.
    [[nodiscard]] bool IsThreaded() const noexcept
    {
      return m_thread.joinable();
    }

  private:
    void Run() noexcept;
    //! Write what is waiting. Called without the mutex held.
    void WritePending(std::array<std::string, StreamCount>& rScratch) noexcept;
  };
}

#endif
