#ifndef FSLDEMOSERVICE_TRACE_IMPL_TRACESERVICE_HPP
#define FSLDEMOSERVICE_TRACE_IMPL_TRACESERVICE_HPP
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


#include <FslDemoService/Trace/Control/ITraceServiceControl.hpp>
#include <FslDemoService/Trace/ITraceService.hpp>
#include <FslDemoService/Trace/Impl/ITraceSink.hpp>
#include <FslService/Impl/ServiceType/Local/ThreadLocalService.hpp>
#include <memory>

namespace Fsl
{
  class IAppInfoService;
  class TraceLog;
  class TraceServiceOptionParser;

  //! The trace service. It records on the thread it was created on (the main thread of the app), the trace is written by a thread
  //! of its own.
  class TraceService final
    : public ThreadLocalService
    , public ITraceService
    , public ITraceServiceControl
  {
    //! The trace (null: nothing is recorded)
    std::unique_ptr<TraceLog> m_log;
    bool m_anonymise;
    std::shared_ptr<IAppInfoService> m_appInfo;
    bool m_processNameWritten{false};

  public:
    //! @param sinkCreator creates what writes the trace file (empty on a platform that has nothing that does).
    TraceService(const ServiceProvider& serviceProvider, const std::shared_ptr<TraceServiceOptionParser>& optionParser,
                 const TraceSinkCreator& sinkCreator);
    ~TraceService() final;

    void Link(const ServiceProvider& serviceProvider) final;

    // From ITraceService
    [[nodiscard]] bool IsEnabled() const noexcept final;
    [[nodiscard]] bool IsAnonymised() const noexcept final;
    [[nodiscard]] TraceFrameIndex GetFrameIndex() const noexcept final;
    TraceZone RegisterZone(const std::string_view name) final;
    void BeginZone(const TraceZone zone) noexcept final;
    void EndZone() noexcept final;
    void BeginZoneAt(const TraceZone zone, const TickCount time) noexcept final;
    void EndZoneAt(const TickCount time) noexcept final;
    TraceTrack RegisterTrack(const std::string_view name, const TraceTrackKind kind) final;
    TraceValue RegisterValue(const std::string_view name, const TraceUnit unit, const std::string_view description) final;
    [[nodiscard]] TraceValue FindValue(const std::string_view name) const noexcept final;
    bool DeclareSpan(const std::string_view title, const TraceTrack track, const TraceValue begin, const TraceValue end, const TraceLink link) final;
    bool DeclareMark(const std::string_view title, const TraceTrack track, const TraceValue time, const TraceLink link) final;
    bool DeclareCounter(const std::string_view title, const TraceValue value) final;
    void SetInt64(const TraceValue value, const int64_t number) noexcept final;
    void SetUInt64(const TraceValue value, const uint64_t number) noexcept final;
    void SetInt64At(const TraceFrameIndex frameIndex, const TraceValue value, const int64_t number) noexcept final;
    void SetUInt64At(const TraceFrameIndex frameIndex, const TraceValue value, const uint64_t number) noexcept final;
    void SetValue(const TraceValue value, const TickCount time) noexcept final;
    void SetValue(const TraceValue value, const NanosecondTickCount time) noexcept final;
    void SetValue(const TraceValue value, const TimeSpan duration) noexcept final;
    void SetValue(const TraceValue value, const NanosecondTimeSpan duration) noexcept final;
    void SetValue(const TraceValue value, const bool flag) noexcept final;
    void SetValueAt(const TraceFrameIndex frameIndex, const TraceValue value, const TickCount time) noexcept final;
    void SetValueAt(const TraceFrameIndex frameIndex, const TraceValue value, const NanosecondTickCount time) noexcept final;
    void SetValueAt(const TraceFrameIndex frameIndex, const TraceValue value, const TimeSpan duration) noexcept final;
    void SetValueAt(const TraceFrameIndex frameIndex, const TraceValue value, const NanosecondTimeSpan duration) noexcept final;
    void AddEvent(const std::string_view name, const std::string_view details) final;
    void SetFact(const std::string_view key, const std::string_view value) final;
    void AddAnonymousText(const std::string_view text, const std::string_view replacement) final;
    void AddAnonymousFact(const std::string_view key, const std::string_view replacement) final;

    // From ITraceServiceControl
    void BeginFrame(const TraceFrameIndex frameIndex, const TraceRunId runId) final;
    void SetFrameBounds(const TraceValue begin, const TraceValue end) final;

  private:
    void WriteFacts();
  };
}

#endif
