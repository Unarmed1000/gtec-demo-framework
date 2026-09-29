#ifndef FSLDEMOSERVICE_FRAMEPACING_FRAMEPACINGMARKERINFO_HPP
#define FSLDEMOSERVICE_FRAMEPACING_FRAMEPACINGMARKERINFO_HPP
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
#include <FslDemoService/FramePacing/FramePacingMarkerKind.hpp>
#include <FslDemoService/FramePacing/FramePacingSequenceId.hpp>
#include <chrono>
#include <cstdint>
#include <optional>

namespace Fsl
{
  //! Every value the last drawn frame pacing marker carried (the mb-framepacing marker payload). A value the framework does not know is
  //! empty, exactly like the marker reports it as unknown.
  struct FramePacingMarkerInfo
  {
    //! The kind of the main marker
    FramePacingMarkerKind Kind{FramePacingMarkerKind::Frame};
    //! The number of frames rendered before this one
    uint64_t FrameIndex{0};
    //! The time the frame's animation was evaluated for
    TimeSpan AnimationTime;
    //! The id of the current (or last) run
    uint32_t RunId{0};
    //! When the frame pacer intends the frame to be shown (always empty: the framework has no frame pacer)
    std::optional<TickCount> IntendedDisplayTime;
    //! The frame time the frame pacer aims for (always empty: the framework has no frame pacer)
    std::optional<TimeSpan> TargetFrameTime;
    //! When the CPU started working on the frame (a HighResolutionTimer timestamp taken before the app update)
    std::optional<TickCount> CpuStartTime;
    //! How long the CPU worked on the frame before the marker was drawn (the last thing before the frame is presented)
    std::optional<TimeSpan> CpuBusyTime;
    //! Start markers only: the wall clock start time of the run
    std::optional<std::chrono::system_clock::time_point> RunStartTime;
    //! Start markers only: the sequence id of the run
    std::optional<FramePacingSequenceId> RunSequenceId;
    //! True if the sync marker (the frame index only) was drawn as well
    bool SyncMarker{false};
  };
}

#endif
