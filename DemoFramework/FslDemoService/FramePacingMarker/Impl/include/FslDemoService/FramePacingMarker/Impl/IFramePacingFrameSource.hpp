#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_IFRAMEPACINGFRAMESOURCE_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_IFRAMEPACINGFRAMESOURCE_HPP
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

#include <FslBase/Time/NanosecondTickCount.hpp>
#include <FslBase/Time/NanosecondTimeSpan.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingMarkerInfo.hpp>
#include <FslDemoService/FramePacingMarker/Impl/FramePacingFrameRecord.hpp>
#include <FslDemoService/Trace/TraceTypes.hpp>
#include <span>
#include <string_view>

namespace Fsl
{
  //! Gives the overlay the marker of the current frame (internal to the frame pacing implementation).
  class IFramePacingFrameSource
  {
  public:
    virtual ~IFramePacingFrameSource() = default;

    //! @brief Get the marker for the current frame.
    //! @return false if no marker should be drawn.
    virtual bool TryGetFrameRecord(FramePacingFrameRecord& rRecord) const noexcept = 0;

    //! @brief Called by the overlay with what a trace of the app is to have of the frame whose marker it just drew: the frame record
    //!        the SDK of the marker makes from the payload of that marker. It is logged as it is given.
    //! @param name the name of the record.
    //! @param startTime when the record begins: the CPU start time of the marker.
    //! @param duration how long it lasts: the CPU busy time of the marker.
    //! @param arguments the other values of the marker the record has, in its order.
    virtual void AddMarkerFrameRecord(const std::string_view name, const NanosecondTickCount startTime, const NanosecondTimeSpan duration,
                                      const std::span<const TraceArgument> arguments) noexcept = 0;

    //! @brief Called by the overlay with every value the marker it just drew carried.
    virtual void SetLastMarker(const FramePacingMarkerInfo& markerInfo) noexcept = 0;
  };
}

#endif
