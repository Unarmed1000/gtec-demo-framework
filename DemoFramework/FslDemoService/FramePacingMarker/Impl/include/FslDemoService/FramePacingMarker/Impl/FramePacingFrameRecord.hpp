#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_FRAMEPACINGFRAMERECORD_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_FRAMEPACINGFRAMERECORD_HPP
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

#include <FslDemoService/FramePacingMarker/FramePacingMarkerKind.hpp>
#include <FslDemoService/FramePacingMarker/FramePacingSequenceId.hpp>
#include <chrono>
#include <cstdint>

namespace Fsl
{
  //! Everything needed to draw the marker of the current frame.
  struct FramePacingFrameRecord
  {
    FramePacingMarkerKind Kind{FramePacingMarkerKind::Frame};
    //! The number of frames rendered before this one.
    uint64_t FrameIndex{0};
    //! The animation time of the frame in nanoseconds.
    int64_t AnimationNanoseconds{0};
    //! When the CPU started working on the frame in nanoseconds (a HighResolutionTimer timestamp), 0 if unknown.
    int64_t CpuStartNanoseconds{0};
    //! How long the CPU has worked on the frame in nanoseconds as the app gave it, 0 if it did not: it is then measured when the marker
    //! is drawn.
    int64_t CpuBusyNanoseconds{0};
    //! When the frame pacer intends the frame to be shown in nanoseconds (on the clock of the HighResolutionTimer), 0 if unknown.
    int64_t IntendedDisplayNanoseconds{0};
    //! The frame time the frame pacer aims for in nanoseconds, 0 if unknown.
    int64_t TargetFrameNanoseconds{0};
    //! The frame time the application wants to run at in nanoseconds, 0 if unknown.
    int64_t PreferredFrameNanoseconds{0};
    //! True if nothing animates while the frame is on screen (the app said so).
    bool Static{false};
    //! True if nothing animated while the frame before this one was on screen (the service found it has the same animation time).
    bool StaticBefore{false};
    uint32_t RunId{0};
    //! Start markers only: the wall clock start time of the run.
    std::chrono::system_clock::time_point RunStartTime;
    //! Start markers only: the sequence id of the run (16 random bytes).
    FramePacingSequenceId RunSequenceId;
    //! Draw the sync marker at the bottom left as well
    bool SyncMarkerEnabled{false};
    int32_t ModuleSizePx{0};
    //! 0 if unknown
    int32_t CaptureHeightPx{0};
  };
}

#endif
