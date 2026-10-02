#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_FRAMEPACINGFRAMESCHEDULE_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_FRAMEPACINGFRAMESCHEDULE_HPP
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
#include <optional>

namespace Fsl
{
  //! The pacing values of the frame being drawn, as an app with its own frame pacer knows them (the framework has no frame pacer).
  //! An app supplies them with IFramePacingMarkerService::SetFrameSchedule so the marker reports what the app actually paced the frame by.
  //! A value the app does not know is left empty, the marker then reports it as unknown.
  struct FramePacingFrameSchedule
  {
    //! The time the frame's animation is evaluated for
    TimeSpan AnimationTime;
    //! When the CPU started working on the frame (a HighResolutionTimer timestamp). Empty: the timestamp the host took before the app update.
    std::optional<TickCount> CpuStartTime;
    //! When the frame pacer intends the frame to be shown (a HighResolutionTimer timestamp)
    std::optional<TickCount> IntendedDisplayTime;
    //! The frame time the frame pacer aims for
    std::optional<TimeSpan> TargetFrameTime;
    //! The frame time the application wants to run at
    std::optional<TimeSpan> PreferredFrameTime;
    //! True if nothing animates while this frame is on screen
    bool Static{false};
  };
}

#endif
