#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_FRAMEPACINGFRAMERECORDUTIL_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_FRAMEPACINGFRAMERECORDUTIL_HPP
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

#include <FslDemoService/FramePacingMarker/FramePacingFrameSchedule.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingMarkerInfo.hpp>
#include <FslDemoService/FramePacingMarker/Impl/FramePacingFrameRecord.hpp>
#include <cstdint>
#include <optional>

namespace Fsl::FramePacingFrameRecordUtil
{
  //! A optional time as the ticks the marker carries: 0 (unknown) if it is empty or not a positive time
  template <typename T>
  constexpr int64_t ToKnownTicks(const std::optional<T>& value) noexcept
  {
    return (value.has_value() && value->Ticks() > 0) ? value->Ticks() : 0;
  }

  //! Replace the pacing values of the record with the ones the app supplied for the frame.
  //! The animation time is always taken from the schedule. The CPU start time of the record (the host's) is kept if the schedule has none.
  constexpr void ApplySchedule(FramePacingFrameRecord& rRecord, const FramePacingFrameSchedule& schedule) noexcept
  {
    rRecord.AnimationTicks = schedule.AnimationTime.Ticks();
    if (schedule.CpuStartTime.has_value())
    {
      rRecord.CpuStartTicks = ToKnownTicks(schedule.CpuStartTime);
    }
    rRecord.IntendedDisplayTicks = ToKnownTicks(schedule.IntendedDisplayTime);
    rRecord.TargetFrameTicks = ToKnownTicks(schedule.TargetFrameTime);
    rRecord.PreferredFrameTicks = ToKnownTicks(schedule.PreferredFrameTime);
    rRecord.Static = schedule.Static;
  }

  //! True if nothing animated while the frame before the record's frame was on screen: the last marker that was drawn is the one of the
  //! frame right before it in the same run, and it has the same animation time (a paused app, or one that only animates on demand).
  //! The record must hold the animation time the frame is drawn with (so a schedule has to be applied first).
  constexpr bool IsStaticBefore(const FramePacingFrameRecord& record, const FramePacingMarkerInfo& lastMarker) noexcept
  {
    return lastMarker.RunId == record.RunId && (lastMarker.FrameIndex + 1u) == record.FrameIndex &&
           lastMarker.AnimationTime.Ticks() == record.AnimationTicks;
  }
}

#endif
