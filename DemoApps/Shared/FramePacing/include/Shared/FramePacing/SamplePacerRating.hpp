#ifndef SHARED_FRAMEPACING_SAMPLEPACERRATING_HPP
#define SHARED_FRAMEPACING_SAMPLEPACERRATING_HPP
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


#include <Shared/FramePacing/SamplePacerTier.hpp>

namespace Fsl
{
  //! What the capabilities of a app are worth, as the pacer library rates them (its PacerRating)
  struct SamplePacerRating
  {
    //! The best tier the app reaches
    SamplePacerTier Tier{SamplePacerTier::TimerPeriodOnly};
    //! The present can hold a frame for two refreshes or more (a time, a minimum duration, or a swap interval of two or more)
    bool DisplaySideHolds{false};
    //! The app reports when its frames were shown: the "+" after the tier's number
    bool ReportsDisplayTimes{false};
    //! The display shows the newer of two frames that are both due and never the older: the "s" after the tier's number. It describes
    //! the system and changes no tier.
    bool SkipsOverdue{false};

    constexpr bool operator==(const SamplePacerRating&) const noexcept = default;
  };
}

#endif
