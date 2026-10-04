#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_FRAMEPACINGFRAMELOG_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_FRAMEPACINGFRAMELOG_HPP
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


#include <FslBase/IO/Path.hpp>
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingFrameLogTable.hpp>
#include <FslDemoService/FramePacingMarker/Impl/Log/FramePacingLogWriter.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace Fsl
{
  //! The frame pacing log: the table of the rows that can still change, the text of the rows that can not, and the writer.
  //! This is what the frame pacing marker service uses to be a IFramePacingFrameLog.
  class FramePacingFrameLog final
  {
    HighResolutionTimer m_timer;
    FramePacingFrameLogTable m_table;
    FramePacingLogWriter m_writer;
    //! Used again for every row, so writing a row does not allocate
    std::string m_text;
    FramePacingLogRow m_closedRow;
    uint64_t m_frameIndex{0};
    bool m_headerWritten{false};
    bool m_isClosed{false};

  public:
    //! @param sink where the text goes (required).
    //! @param useThread false to write everything when the log is closed.
    explicit FramePacingFrameLog(std::unique_ptr<IFramePacingLogSink> sink, const bool useThread = true);
    ~FramePacingFrameLog();

    //! @brief Create a log that writes to a file and the events file next to it.
    //! @return the log, null if the files could not be created (the reason is logged).
    static std::unique_ptr<FramePacingFrameLog> TryCreate(const IO::Path& framesPath);

    FramePacingLogColumn RegisterColumn(const std::string_view name, const FramePacingLogUnit unit, const std::string_view description);

    //! @brief A frame begins: its row is opened and the row that is too old to change is written.
    void BeginFrame(const uint64_t frameIndex);

    [[nodiscard]] uint64_t GetFrameIndex() const noexcept
    {
      return m_frameIndex;
    }

    void SetInt64(const FramePacingLogColumn column, const int64_t value) noexcept
    {
      m_table.SetValue(m_frameIndex, column, value, false);
    }

    void SetUInt64(const FramePacingLogColumn column, const uint64_t value) noexcept
    {
      m_table.SetValue(m_frameIndex, column, static_cast<int64_t>(value), true);
    }

    void SetInt64At(const uint64_t frameIndex, const FramePacingLogColumn column, const int64_t value) noexcept
    {
      m_table.SetValue(frameIndex, column, value, false);
    }

    void SetUInt64At(const uint64_t frameIndex, const FramePacingLogColumn column, const uint64_t value) noexcept
    {
      m_table.SetValue(frameIndex, column, static_cast<int64_t>(value), true);
    }

    void AddEvent(const std::string_view name, const std::string_view details);
    void AddFact(const std::string_view key, const std::string_view value);

    //! @brief Write the rows that are still open and stop.
    void Close() noexcept;

    //! @return true if writing failed, the log is then incomplete.
    [[nodiscard]] bool HasFailed() const noexcept
    {
      return m_writer.HasFailed();
    }

  private:
    void WriteHeaderIfNeeded();
    void WriteRow(const FramePacingLogRow& row);
  };
}

#endif
