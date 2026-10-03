#ifndef SHARED_PIXELART_BASE_PIXELARTFRAMESTATE_HPP
#define SHARED_PIXELART_BASE_PIXELARTFRAMESTATE_HPP
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

#include <FslBase/Math/Pixel/PxSize2D.hpp>
#include <FslBase/Span/ReadOnlySpan.hpp>
#include <array>
#include <cstdint>

namespace Fsl
{
  //! What the renderer draws a frame of the scene with: the values of the shadertoy uniforms
  struct PixelArtFrameState
  {
    float Time{0.0f};
    float TimeDelta{0.0f};
    float FrameRate{0.0f};
    //! 0 on the first frame of the scene (the renderer clears the buffers)
    int32_t Frame{0};
    //! iMouse in the pixels of the scene area with the origin in its bottom left corner: xy = the position while a button is down, zw = where
    //! it went down (z is negative once it is up, w is negative except on the frame it went down)
    std::array<float, 4> MouseWindowPx{};
    //! year, month (0-11), day (1-31), seconds since midnight
    std::array<float, 4> Date{};
    //! The part of the window the scene is shown in: from the top left corner of the window to the left edge of the UI panel (all of the
    //! window when the panel is hidden). The image pass draws to it, it is the iResolution of the image pass.
    PxSize2D SceneSizePx;
    //! The size of the buffers (the scene size scaled by the render scale)
    PxSize2D RenderSizePx;
    //! The adjustable constants (see PixelArtParamSet::GetPackedValues)
    ReadOnlySpan<float> Params;
  };
}

#endif
