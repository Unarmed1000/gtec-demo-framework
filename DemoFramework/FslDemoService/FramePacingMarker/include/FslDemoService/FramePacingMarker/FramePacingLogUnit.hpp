#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_FRAMEPACINGLOGUNIT_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_FRAMEPACINGLOGUNIT_HPP
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


namespace Fsl
{
  //! What the whole number in a column of the frame pacing log counts. Every value of the log is a whole number, so nothing is rounded.
  enum class FramePacingLogUnit
  {
    //! A moment on the clock of the framework (HighResolutionTimer) in 100ns ticks
    Ticks,
    //! A length of time in 100ns ticks
    DurationTicks,
    //! Nanoseconds as a driver or the operating system reported them (a moment or a length of time on a clock of its own)
    Nanoseconds,
    //! A number of something
    Count,
    //! A identifier
    Id,
    //! Zero or one
    Flag,
    //! Pixels
    Pixels,
    //! A value of a enum or a result code, the description of the column says which
    Code
  };
}

#endif
