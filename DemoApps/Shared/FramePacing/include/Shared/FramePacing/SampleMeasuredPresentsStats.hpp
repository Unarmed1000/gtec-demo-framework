#ifndef SHARED_FRAMEPACING_SAMPLEMEASUREDPRESENTSSTATS_HPP
#define SHARED_FRAMEPACING_SAMPLEMEASUREDPRESENTSSTATS_HPP
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

#include <FslBase/Time/TimeSpan.hpp>
#include <cstdint>
#include <optional>

namespace Fsl
{
  //! What the measured presents of the last frames say (see SampleMeasuredPresents)
  struct SampleMeasuredPresentsStats
  {
    //! The frames the presentation engine reported on
    uint32_t MeasuredFrames{0};
    //! The measured frames the presentation engine reported a display time for. It does not have one for every frame (the time of a stage
    //! can be 'not available'), which does not mean the frame was not shown.
    uint32_t TimedFrames{0};
    //! The timed frames the frame pacer aimed at a display time
    uint32_t PacedFrames{0};
    //! The time from 'the frame pacer aimed for' to 'reached the display' of the paced frames (negative = early)
    std::optional<TimeSpan> AverageDisplayError;
    std::optional<TimeSpan> WorstDisplayError;
    //! The time between two frames in a row reaching the display: how even the frames really are (empty if no two frames in a row were timed)
    std::optional<TimeSpan> AverageDisplayInterval;
    std::optional<TimeSpan> MinDisplayInterval;
    std::optional<TimeSpan> MaxDisplayInterval;
    //! The time from 'the CPU started on the frame' to 'reached the display' of the timed frames
    std::optional<TimeSpan> AverageLatency;
    //! The time from 'the CPU started on the frame' to 'the present was handed to the presentation engine' (empty if not reported)
    std::optional<TimeSpan> AverageQueueTime;
  };
}

#endif
