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
  //! The scenes the background of the FramePacing samples can show. The first two are raymarched and costly from the first step on,
  //! the others are cheap at a low load, and the last one is what a low end GPU can draw.
  enum class RaymarchScene
  {
    //! A flight through a fractal lattice over water
    Flight,
    //! A hall of columns that scrolls sideways at a constant speed, which makes a stutter easy to see
    Hall,
    //! A flight through a field of blobs that melt into each other (sphere traced, cheap steps)
    Blobs,
    //! A lace of circles packed into circles that the animation zooms into and out of. The load adds rounds of finer circles, and
    //! samples per pixel.
    Lace,
    //! The zoom into the Mandelbrot set of the FractalShader, GpuTimestamp and ShaderClock samples. The load is the number of
    //! iterations. The cheapest scene.
    Mandelbrot
  };

  //! What the raymarched background of the FramePacing samples is drawn with (the GPU load of the sample).
  //! The shader is drawn by each app as the shared code only knows the API independent render interfaces. The animation is given as
  //! phases, so the shader gets small exact values no matter how long the app has been running.
  struct RaymarchParams
  {
    //! The GPU load (0 = the background is not drawn): the number of steps the shader marches every ray in. For the lace every
    //! doubling of it is one more round of detail, and the rest of it is the number of samples it draws every pixel with. For the
    //! Mandelbrot set a pixel can take twice this many iterations.
    int32_t Steps{0};
    //! The scene that is drawn
    RaymarchScene Scene{RaymarchScene::Flight};
    //! The travel of the camera in [0,1), it repeats when it wraps: the flight through the lattice, or the way along the hall
    float TravelPhase{0.0f};
    //! Flight: the sway of the camera, the waves on the water and the pulses of light in [0,1). The hall does not use it, its camera
    //! only moves sideways.
    float SwayPhase{0.0f};
    //! Flight: the slow change of the shape and the colors of the lattice in [0,1). The hall does not use it.
    float MorphPhase{0.0f};

    //! The scene as the shader gets it (0 = Flight, 1 = Hall, 2 = Blobs, 3 = Lace, 4 = Mandelbrot)
    [[nodiscard]] constexpr float SceneAsFloat() const noexcept
    {
      switch (Scene)
      {
      case RaymarchScene::Hall:
        return 1.0f;
      case RaymarchScene::Blobs:
        return 2.0f;
      case RaymarchScene::Lace:
        return 3.0f;
      case RaymarchScene::Mandelbrot:
        return 4.0f;
      case RaymarchScene::Flight:
      default:
        return 0.0f;
      }
    }
  };
}

#endif
