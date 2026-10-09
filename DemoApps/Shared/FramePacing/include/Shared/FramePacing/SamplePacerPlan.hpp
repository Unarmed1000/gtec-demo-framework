#ifndef SHARED_FRAMEPACING_SAMPLEPACERPLAN_HPP
#define SHARED_FRAMEPACING_SAMPLEPACERPLAN_HPP
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


#include <FslBase/Time/NanosecondTimeSpan.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <cstdint>
#include <optional>

namespace Fsl
{
  //! What the pacer wants the caller to wait for before a frame takes anything, in this order: a present or the GPU's work on a
  //! frame (never both), then a time. Each can be absent.
  struct SamplePacerFrameStartPlan
  {
    //! The present to wait for until it was shown, by the id the pacer gave its frame (zero: none)
    uint64_t WaitForPresentFrameId{0};
    //! The longest that wait may take
    TimeSpan WaitForPresentTimeout;
    //! The frame to wait for until the GPU is done with it, by the id the pacer gave it (zero: none). It is the one wait of the app
    //! for a frame slot.
    uint64_t WaitForGpuWorkFrameId{0};
    //! The longest that wait may take
    TimeSpan WaitForGpuWorkTimeout;
    //! The time to wait until after that (a HighResolutionTimer timestamp), zero: none, the frame starts at once
    TickCount StartTime;
  };

  //! What became of the wait for a present a SamplePacerFrameStartPlan asked for.
  struct SamplePacerPresentWaitReport
  {
    //! The SamplePacerFrameStartPlan::WaitForPresentFrameId that was waited for
    uint64_t FrameId{0};
    //! When the wait began and when it ended (HighResolutionTimer timestamps)
    TickCount BeginTime;
    TickCount EndTime;
    //! True if the wait ended because the present was shown, false if it ended without that (its time ran out, or the system gave it up)
    bool Shown{true};
  };

  //! How the pacer wants a frame whose CPU work is done to be presented.
  struct SamplePacerPresentPlan
  {
    //! The id the pacer gave the frame (zero = no frame), which the report of its present is given with
    uint64_t FrameId{0};
    //! The time to wait until before the present (a HighResolutionTimer timestamp), zero: none, the frame is presented at once
    TickCount PresentTime;
    //! The swap interval for a present that takes one
    uint32_t SwapInterval{1};
    //! The time before which the frame is not to be shown, for a present that takes one (a HighResolutionTimer timestamp), zero:
    //! none. With it the display places the frame.
    TickCount NotBeforeTime;
    //! The time the frame before this one is to stay on screen at least, for a present that takes one (zero: none). It is given to
    //! the present next to everything else of the plan: the time to wait until is still waited for.
    NanosecondTimeSpan MinimumDuration;
    //! The CPU busy time of the frame as the pacer counts it: from the start of the frame to the end of its CPU work (zero: not known)
    TimeSpan CpuBusy;
  };

  //! The work of the GPU on a frame that was presented earlier, which the pacer is told. It is known frames later.
  struct SamplePacerGpuWorkReport
  {
    //! The SamplePacerPresentPlan::FrameId of the frame
    uint64_t FrameId{0};
    //! When the GPU began and ended the work on the frame (HighResolutionTimer timestamps), empty where the app does not know
    std::optional<TickCount> BeginTime;
    std::optional<TickCount> EndTime;
    //! How long the work took
    TimeSpan Duration;
  };

  //! When the present of a frame was called and when it returned, which the pacer is told.
  struct SamplePacerPresentReport
  {
    //! The SamplePacerPresentPlan::FrameId of the frame
    uint64_t FrameId{0};
    //! When the present was called and when it returned (HighResolutionTimer timestamps)
    TickCount CallTime;
    TickCount ReturnTime;
    //! False if the system did not take the present, so the frame will not be shown
    bool Accepted{true};
  };
}

#endif
