#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_FRAMEPACINGPRESENTRECORD_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_FRAMEPACINGPRESENTRECORD_HPP
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
#include <FslDemoService/FramePacingMarker/FramePacingPresentResult.hpp>
#include <cstdint>
#include <optional>

namespace Fsl
{
  //! The present call of a frame (IFramePacingDetailService::AddPresent). The swap interval it was made with is the one the service was
  //! told last (IFramePacingDetailService::SetSwapInterval). A value that is not known is empty.
  struct FramePacingPresentRecord
  {
    //! When the present was called and when it returned (HighResolutionTimer timestamps)
    TickCount CallTime;
    TickCount ReturnTime;
    FramePacingPresentResult Result{FramePacingPresentResult::Accepted};
    //! The platform's own number for what the present returned (a VkResult)
    std::optional<int64_t> ResultCode;
    //! The time the present was given before which the frame is not shown (a HighResolutionTimer timestamp)
    std::optional<TickCount> NotBefore;
    //! The time the present was given that the frame before stays on screen at least
    std::optional<NanosecondTimeSpan> MinimumDuration;
    //! How many frames were presented and not yet shown or finished when the present was made
    std::optional<uint32_t> FramesInFlight;
  };
}

#endif
