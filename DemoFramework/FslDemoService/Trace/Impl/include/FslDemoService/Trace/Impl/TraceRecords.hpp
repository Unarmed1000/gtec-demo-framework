#ifndef FSLDEMOSERVICE_TRACE_IMPL_TRACERECORDS_HPP
#define FSLDEMOSERVICE_TRACE_IMPL_TRACERECORDS_HPP
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


#include <FslDemoService/Trace/TraceTypes.hpp>
#include <array>
#include <bitset>
#include <cstdint>
#include <string>
#include <vector>

namespace Fsl
{
  //! The values of one frame
  struct TraceFrameRow
  {
    static constexpr uint32_t MaxValues = 256;

    uint64_t FrameIndex{0};
    uint32_t RunId{0};
    std::bitset<MaxValues> HasValue;
    //! A unsigned value is stored as its bits
    std::array<int64_t, MaxValues> Values{};
  };

  struct TraceValueInfo
  {
    std::string Name;
    TraceUnit Unit{TraceUnit::Count};
    std::string Description;
  };

  struct TraceTrackInfo
  {
    std::string Name;
    TraceTrackKind Kind{TraceTrackKind::Sequential};
  };

  //! Two times of a frame that are drawn as a span. The indices are those of TraceSchema::Tracks and TraceSchema::Values.
  struct TraceSpanInfo
  {
    std::string Title;
    uint32_t TrackIndex{0};
    uint32_t BeginIndex{0};
    uint32_t EndIndex{0};
    TraceLink Link{TraceLink::None};
  };

  //! A time of a frame that is drawn as a mark
  struct TraceMarkInfo
  {
    std::string Title;
    uint32_t TrackIndex{0};
    uint32_t TimeIndex{0};
    TraceLink Link{TraceLink::None};
  };

  //! A value of a frame that is drawn as a graph
  struct TraceCounterInfo
  {
    std::string Title;
    uint32_t ValueIndex{0};
  };

  //! What the rows hold and how it is drawn. It does not change once the first row was written.
  struct TraceSchema
  {
    std::vector<TraceValueInfo> Values;
    std::vector<TraceTrackInfo> Tracks;
    std::vector<TraceSpanInfo> Spans;
    std::vector<TraceMarkInfo> Marks;
    std::vector<TraceCounterInfo> Counters;
    //! The two times a frame lasts from and to (indices of Values)
    bool HasFrameBounds{false};
    uint32_t FrameBeginIndex{0};
    uint32_t FrameEndIndex{0};
  };

  //! The begin or the end of a zone, in the order they happened
  struct TraceZoneRecord
  {
    int64_t Ticks{0};
    //! The number of the zone counted from one (zero for a end: it ends the zone that began last)
    uint32_t Zone{0};
    bool IsBegin{false};
  };

  struct TraceEventRecord
  {
    int64_t Ticks{0};
    uint64_t FrameIndex{0};
    uint32_t RunId{0};
    //! False if no frame had begun when it was written
    bool HasFrame{false};
    //! True: Name is the key and Details the value of a fact
    bool IsFact{false};
    std::string Name;
    std::string Details;
  };
}

#endif
