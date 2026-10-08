#ifndef FSLDEMOSERVICE_TRACE_ITRACESERVICE_HPP
#define FSLDEMOSERVICE_TRACE_ITRACESERVICE_HPP
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


#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoService/Trace/TraceTypes.hpp>
#include <cstdint>
#include <string_view>

namespace Fsl
{
  //! The trace of the app (--Trace <file>): what its threads were doing and what is known about every frame, written to a file a trace
  //! viewer opens and other tools can query and add to.
  //!
  //! There are three kinds of things to record:
  //! - A zone is a scope on the thread: a update, a draw, a wait. Begin it and end it (ScopedTraceZone does both), as often as it occurs.
  //! - A frame value is a number of one frame: a time, a duration, a id, a result. Register it once and set it each frame. It can be set
  //!   for a earlier frame as well, as some things are only known later (when a frame reached the display).
  //! - A event is something that happened at a moment, a fact something that holds for the whole run.
  //!
  //! What a frame value looks like on the timeline is declared once: two times are a span, a time is a mark, a count is a counter. The
  //! declaration only decides how it is drawn, the value is always written as the number it was set to.
  //!
  //! Everything here does nothing while the trace is off, so there is no need to check for it. Use IsEnabled to skip the work of finding
  //! a value out. The service is used from the thread it was created on.
  class ITraceService
  {
  public:
    virtual ~ITraceService() = default;

    //! @brief Check if the trace is being written.
    [[nodiscard]] virtual bool IsEnabled() const noexcept = 0;

    //! @brief Check if the names of the hardware and the directories of the machine are kept out of the trace (the default).
    [[nodiscard]] virtual bool IsAnonymised() const noexcept = 0;

    //! @brief The index of the frame that is being drawn.
    [[nodiscard]] virtual TraceFrameIndex GetFrameIndex() const noexcept = 0;

    // Zones

    //! @brief Add the name of a zone. A name that exists returns that zone.
    //! @return the zone (not valid if the trace is off or the name is empty).
    virtual TraceZone RegisterZone(const std::string_view name) = 0;

    //! @brief A zone begins on this thread, now. Zones are ended in the reverse order they began in.
    virtual void BeginZone(const TraceZone zone) noexcept = 0;

    //! @brief The zone that began last on this thread ends, now.
    virtual void EndZone() noexcept = 0;

    //! @brief A zone begins at a time that was read already. The times given must not go back.
    virtual void BeginZoneAt(const TraceZone zone, const TickCount time) noexcept = 0;

    //! @brief The zone that began last on this thread ends at a time that was read already.
    virtual void EndZoneAt(const TickCount time) noexcept = 0;

    // Frame values

    //! @brief Add a track. A name that exists returns that track.
    //! @return the track (not valid if the trace is off or the name is empty).
    virtual TraceTrack RegisterTrack(const std::string_view name, const TraceTrackKind kind) = 0;

    //! @brief Add a value of a frame. Values can be added until the first frames are written, so do it at startup.
    //! @param name the name of the value: letters, digits, '_' and '.'. A name that exists returns that value.
    //! @param unit what the value counts.
    //! @param description what the value is.
    //! @return the value (not valid if the trace is off or the value could not be added).
    virtual TraceValue RegisterValue(const std::string_view name, const TraceUnit unit, const std::string_view description) = 0;

    //! @brief Find a value somebody registered.
    //! @return the value (not valid if there is none of that name).
    [[nodiscard]] virtual TraceValue FindValue(const std::string_view name) const noexcept = 0;

    //! @brief Two times of a frame are drawn as a span on a track. A frame that lacks one of the two has no span.
    //! @param begin a value of the unit TraceUnit::Ticks.
    //! @param end a value of the unit TraceUnit::Ticks.
    //! @return false if it was not declared (the trace is off, a value is not a time, or it is too late).
    virtual bool DeclareSpan(const std::string_view title, const TraceTrack track, const TraceValue begin, const TraceValue end,
                             const TraceLink link) = 0;

    //! @brief A time of a frame is drawn as a mark on a track.
    //! @param time a value of the unit TraceUnit::Ticks.
    //! @return false if it was not declared.
    virtual bool DeclareMark(const std::string_view title, const TraceTrack track, const TraceValue time, const TraceLink link) = 0;

    //! @brief A value of a frame is drawn as a graph, with a point where each frame began.
    //! @return false if it was not declared.
    virtual bool DeclareCounter(const std::string_view title, const TraceValue value) = 0;

    //! @brief Set a value of the frame that is being drawn.
    virtual void SetInt64(const TraceValue value, const int64_t number) noexcept = 0;
    virtual void SetUInt64(const TraceValue value, const uint64_t number) noexcept = 0;

    //! @brief Set a value of a earlier frame. It is ignored if the frame was written already (a frame is kept for 64 frames).
    virtual void SetInt64At(const TraceFrameIndex frameIndex, const TraceValue value, const int64_t number) noexcept = 0;
    virtual void SetUInt64At(const TraceFrameIndex frameIndex, const TraceValue value, const uint64_t number) noexcept = 0;

    // Events and facts

    //! @brief Write a event: something that happened now and is not a value of every frame (a swapchain was created, a setting was
    //!        changed).
    //! @param name the name of the event.
    //! @param details what happened, by convention "key=value;key=value".
    virtual void AddEvent(const std::string_view name, const std::string_view details) = 0;

    //! @brief Write a fact about the run (the graphics API, the present mode, a setting).
    virtual void SetFact(const std::string_view key, const std::string_view value) = 0;

    //! @brief Name a text that says which hardware this is (the model of the graphics device) and what is to be written in its place
    //!        while the trace is anonymised. It is replaced in every event and fact that is written from now on.
    virtual void AddAnonymousText(const std::string_view text, const std::string_view replacement) = 0;

    //! @brief Name a fact that says which hardware this is (the id of the graphics device) and the value that is to be written for it
    //!        while the trace is anonymised. Do it before the fact is set.
    virtual void AddAnonymousFact(const std::string_view key, const std::string_view replacement) = 0;

    // The typed forms, so a time is never mistaken for a duration

    void SetValue(const TraceValue value, const TickCount time) noexcept
    {
      SetInt64(value, time.Ticks());
    }

    void SetValue(const TraceValue value, const TimeSpan duration) noexcept
    {
      SetInt64(value, duration.Ticks());
    }

    void SetValue(const TraceValue value, const bool flag) noexcept
    {
      SetInt64(value, flag ? 1 : 0);
    }

    void SetValueAt(const TraceFrameIndex frameIndex, const TraceValue value, const TickCount time) noexcept
    {
      SetInt64At(frameIndex, value, time.Ticks());
    }

    void SetValueAt(const TraceFrameIndex frameIndex, const TraceValue value, const TimeSpan duration) noexcept
    {
      SetInt64At(frameIndex, value, duration.Ticks());
    }
  };
}

#endif
