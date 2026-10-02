#ifndef SHARED_FRAMEPACING_RAYMARCHPARAMS_HPP
#define SHARED_FRAMEPACING_RAYMARCHPARAMS_HPP
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
  //! What the raymarched background of the FramePacing samples is drawn with (the GPU load of the sample).
  //! The shader is drawn by each app as the shared code only knows the API independent render interfaces. The animation is given as
  //! phases, so the shader gets small exact values no matter how long the app has been running.
  struct RaymarchParams
  {
    //! The number of steps the shader marches every ray in: the GPU load (0 = the background is not drawn)
    int32_t Steps{0};
    //! The flight through the lattice in [0,1), the flight repeats when it wraps
    float TravelPhase{0.0f};
    //! The sway of the camera, the waves on the water and the pulses of light in [0,1)
    float SwayPhase{0.0f};
    //! The slow change of the shape and the colors of the lattice in [0,1)
    float MorphPhase{0.0f};
  };
}

#endif
