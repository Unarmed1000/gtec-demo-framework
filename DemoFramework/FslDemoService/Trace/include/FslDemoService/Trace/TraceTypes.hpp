#ifndef FSLDEMOSERVICE_TRACE_TRACETYPES_HPP
#define FSLDEMOSERVICE_TRACE_TRACETYPES_HPP
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


#include <cstdint>

namespace Fsl
{
  //! The index of a frame in the trace. With the frame pacing marker service it is the frame index the marker of the frame carries.
  struct TraceFrameIndex
  {
    uint64_t Value{0};

    constexpr TraceFrameIndex() noexcept = default;
    constexpr explicit TraceFrameIndex(const uint64_t value) noexcept
      : Value(value)
    {
    }

    constexpr bool operator==(const TraceFrameIndex& rhs) const noexcept = default;
  };


  //! The id of the run a frame belongs to. With the frame pacing marker service it is the run id the marker of the frame carries.
  struct TraceRunId
  {
    uint32_t Value{0};

    constexpr TraceRunId() noexcept = default;
    constexpr explicit TraceRunId(const uint32_t value) noexcept
      : Value(value)
    {
    }

    constexpr bool operator==(const TraceRunId& rhs) const noexcept = default;
  };


  //! A value of a frame (ITraceService::RegisterValue). The default one is not a value, and setting it does nothing, which is what a
  //! registration returns while the trace is off.
  struct TraceValue
  {
    //! Zero = not a value, else the number of the value counted from one
    uint32_t Value{0};

    constexpr TraceValue() noexcept = default;
    constexpr explicit TraceValue(const uint32_t value) noexcept
      : Value(value)
    {
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
      return Value != 0u;
    }

    constexpr bool operator==(const TraceValue& rhs) const noexcept = default;
  };


  //! A track of the trace: a named row of the timeline (ITraceService::RegisterTrack). The default one is not a track.
  struct TraceTrack
  {
    //! Zero = not a track, else the number of the track counted from one
    uint32_t Value{0};

    constexpr TraceTrack() noexcept = default;
    constexpr explicit TraceTrack(const uint32_t value) noexcept
      : Value(value)
    {
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
      return Value != 0u;
    }

    constexpr bool operator==(const TraceTrack& rhs) const noexcept = default;
  };


  //! The name of a zone: a scope on a thread (ITraceService::RegisterZone). The default one is not a zone, and beginning it does nothing.
  struct TraceZone
  {
    //! Zero = not a zone, else the number of the zone counted from one
    uint32_t Value{0};

    constexpr TraceZone() noexcept = default;
    constexpr explicit TraceZone(const uint32_t value) noexcept
      : Value(value)
    {
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
      return Value != 0u;
    }

    constexpr bool operator==(const TraceZone& rhs) const noexcept = default;
  };


  //! What the values of a frame value count
  enum class TraceUnit
  {
    //! A moment on the steady clock of the framework (HighResolutionTimer) in 100 nanosecond ticks
    Ticks,
    //! A moment on the steady clock of the framework in nanoseconds (NanosecondTickCount). It can be drawn as a mark
    NanosecondTicks,
    //! A duration in 100 nanosecond ticks
    DurationTicks,
    //! A duration in nanoseconds, or a moment in nanoseconds on a clock that is not the one of the framework: a driver or the window
    //! system has a clock of its own
    Nanoseconds,
    //! A number of things
    Count,
    //! A id or a index
    Id,
    //! 0 or 1
    Flag,
    //! Pixels
    Pixels,
    //! One of a set of numbers that each have a meaning (a result code, a kind)
    Code,
  };


  //! How the spans of a track relate to each other
  enum class TraceTrackKind
  {
    //! The spans follow each other or are inside each other, like the scopes of a thread
    Sequential,
    //! The spans of frames can overlap (the GPU works on a frame while the next one is presented). They are drawn in lanes below each other
    Lanes,
  };


  //! If a span or a mark is part of the chain of its frame
  enum class TraceLink
  {
    //! It is not a step of the chain of its frame
    NoLink,
    //! It is one of the steps a frame goes through: the steps of a frame are linked in the order of their times
    FrameChain,
  };
}

#endif
