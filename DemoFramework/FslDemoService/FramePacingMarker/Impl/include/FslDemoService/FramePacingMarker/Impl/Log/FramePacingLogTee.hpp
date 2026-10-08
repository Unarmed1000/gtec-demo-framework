#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_FRAMEPACINGLOGTEE_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_LOG_FRAMEPACINGLOGTEE_HPP
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
#include <FslDemoService/FramePacingMarker/FramePacingLogColumn.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingLogUnit.hpp>
#include <FslDemoService/Trace/TraceTypes.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Fsl
{
  class FramePacingFrameLog;
  class ITraceService;
  class ITraceServiceControl;

  //! What is logged about the frames goes to two places: the frame pacing log (--FramePacing.Log, the CSV files) and the trace of the
  //! trace service (--Trace). This hands every column, value, event and fact to the ones that are on, so what logs does it once and
  //! does not know where it goes. The frames of the trace begin here, with the frame index and the run id of the marker.
  class FramePacingLogTee final
  {
    struct ColumnRecord
    {
      std::string Name;
      FramePacingLogColumn LogColumn;
      TraceValue Value;
    };

    //! The frame pacing log (null: off)
    std::unique_ptr<FramePacingFrameLog> m_log;
    //! The trace (null: off)
    std::shared_ptr<ITraceService> m_trace;
    std::shared_ptr<ITraceServiceControl> m_traceControl;
    //! The columns that were handed out, a column is its place here counted from one
    std::vector<ColumnRecord> m_columns;
    uint64_t m_frameIndex{0};

  public:
    FramePacingLogTee(const FramePacingLogTee&) = delete;
    FramePacingLogTee& operator=(const FramePacingLogTee&) = delete;

    FramePacingLogTee(std::unique_ptr<FramePacingFrameLog> log, std::shared_ptr<ITraceService> trace,
                      std::shared_ptr<ITraceServiceControl> traceControl);
    ~FramePacingLogTee();

    //! @brief Create the tee for the places that are on.
    //! @param logPath the file of the frame pacing log (empty: off).
    //! @param trace the trace service (null, or not enabled: off).
    //! @return the tee, null if neither is on.
    static std::unique_ptr<FramePacingLogTee> TryCreate(const IO::Path& logPath, const std::shared_ptr<ITraceService>& trace,
                                                        const std::shared_ptr<ITraceServiceControl>& traceControl);

    FramePacingLogColumn RegisterColumn(const std::string_view name, const FramePacingLogUnit unit, const std::string_view description);

    //! @brief A frame begins, in the log and in the trace.
    void BeginFrame(const uint64_t frameIndex, const uint32_t runId);

    //! @brief Name the two columns a frame lasts from and to in the trace. A name that is not a column is ignored.
    void SetTraceFrameBounds(const std::string_view beginColumnName, const std::string_view endColumnName);

    void SetInt64(const FramePacingLogColumn column, const int64_t value) noexcept
    {
      SetInt64At(m_frameIndex, column, value);
    }

    void SetUInt64(const FramePacingLogColumn column, const uint64_t value) noexcept
    {
      SetUInt64At(m_frameIndex, column, value);
    }

    void SetInt64At(const uint64_t frameIndex, const FramePacingLogColumn column, const int64_t value) noexcept;
    void SetUInt64At(const uint64_t frameIndex, const FramePacingLogColumn column, const uint64_t value) noexcept;
    void AddEvent(const std::string_view name, const std::string_view details);
    void AddFact(const std::string_view key, const std::string_view value);

    //! @brief Write the rows of the log that are still open and stop it. The trace is closed by its service.
    void Close() noexcept;
  };
}

#endif
