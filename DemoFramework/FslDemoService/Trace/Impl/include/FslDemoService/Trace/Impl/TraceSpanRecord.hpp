#ifndef FSLDEMOSERVICE_TRACE_IMPL_TRACESPANRECORD_HPP
#define FSLDEMOSERVICE_TRACE_IMPL_TRACESPANRECORD_HPP
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

#include <array>
#include <cstddef>
#include <cstdint>

namespace Fsl
{
  //! A span that was added with its times and its texts (ITraceService::AddSpan), as it waits for the sink. The texts are held in the
  //! record and each ends with a zero, so adding a span allocates nothing. What a span can hold is what can be added.
  struct TraceSpanRecord
  {
    static constexpr std::size_t MaxCategoryLength = 15;
    static constexpr std::size_t MaxNameLength = 63;
    static constexpr std::size_t MaxArgumentNameLength = 31;
    static constexpr std::size_t MaxArguments = 8;

    //! The index of the track (TraceSchema::Tracks)
    uint32_t TrackIndex{0};
    std::array<char, MaxCategoryLength + 1> Category{};
    std::array<char, MaxNameLength + 1> Name{};
    //! When it begins on the steady clock of the framework, and how long it lasts
    int64_t BeginNanoseconds{0};
    int64_t DurationNanoseconds{0};
    //! The first ArgumentCount names and values are the arguments of the span, in the order they were given
    std::array<std::array<char, MaxArgumentNameLength + 1>, MaxArguments> ArgumentNames{};
    std::array<int64_t, MaxArguments> ArgumentValues{};
    uint32_t ArgumentCount{0};
  };
}

#endif
