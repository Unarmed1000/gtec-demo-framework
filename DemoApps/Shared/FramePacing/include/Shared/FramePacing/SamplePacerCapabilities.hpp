#ifndef SHARED_FRAMEPACING_SAMPLEPACERCAPABILITIES_HPP
#define SHARED_FRAMEPACING_SAMPLEPACERCAPABILITIES_HPP
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


#include <cstdint>

namespace Fsl
{
  //! What a app can do or can tell a pacer (the capabilities of the pacer library, in the types of the framework). Every app has the
  //! baseline, which is none of them: a clock, the refresh period of the display, a wait until a time and a present. The pacer is
  //! told all of it, and only what is so for the display the window is on.
  struct SamplePacerCapabilities
  {
    //! The longest swap interval the present takes (zero: the present takes none). It only holds a frame for more than one refresh
    //! when it can be two or more.
    uint32_t PresentSwapIntervalMax{0};
    //! The present takes a time before which the frame is not shown, so the display places the frame
    bool PresentAtTime{false};
    //! Of two presents whose times have both passed the display side shows the later one and never the earlier one: a fact of the
    //! system or of the present mode, and only something with PresentAtTime
    bool PresentSkipsOverdue{false};
    //! The present takes a time the frame before it stays on screen at least
    bool PresentAfterDuration{false};
    //! The app knows when a vertical blank was and the period between them
    bool VBlankTimes{false};
    //! The app can wait until a present it names was shown
    bool WaitForPresent{false};
    //! The app can wait until the GPU is done with a frame it names
    bool WaitForGpuWork{false};
    //! The app can say when the GPU began and ended its work on a frame, on its clock, frames later
    bool GpuWorkTimes{false};
    //! The app can say how long the GPU worked on a frame, frames later
    bool GpuWorkDurations{false};
    //! The app is told when a present was shown, frames later
    bool DisplayTimes{false};

    constexpr bool operator==(const SamplePacerCapabilities&) const noexcept = default;
  };
}

#endif
