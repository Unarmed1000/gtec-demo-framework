#ifndef FSLDEMOSERVICE_FRAMEPACING_IMPL_FRAMEPACINGFRAMERECORD_HPP
#define FSLDEMOSERVICE_FRAMEPACING_IMPL_FRAMEPACINGFRAMERECORD_HPP
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

#include <FslBase/String/StringViewLite.hpp>
#include <FslDemoService/FramePacing/FramePacingMarkerSlot.hpp>
#include <FslDemoService/FramePacing/Impl/FramePacingMarkerKind.hpp>
#include <cstdint>

namespace Fsl
{
  //! Everything needed to draw the marker of the current frame.
  struct FramePacingFrameRecord
  {
    FramePacingMarkerKind Kind{FramePacingMarkerKind::Frame};
    //! The number of frames rendered before this one.
    uint64_t FrameIndex{0};
    //! The animation time of the frame in 100ns ticks.
    int64_t AnimationTicks{0};
    uint32_t RunId{0};
    //! Start markers only: the wall clock start time of the run as C# DateTime UTC ticks.
    int64_t StartUtcTicks{0};
    //! Start markers only: the name of the run (valid until the next call to the service).
    StringViewLite RunName;
    FramePacingMarkerSlot Slot{FramePacingMarkerSlot::TopLeft};
    int32_t ModuleSizePx{0};
    //! 0 if unknown
    int32_t CaptureHeightPx{0};
  };
}

#endif
