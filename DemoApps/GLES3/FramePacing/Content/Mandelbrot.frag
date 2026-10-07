#version 300 es
// Copyright (c) 2014 Freescale Semiconductor, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//
//    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
//      its contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// This is Mandelbrot_col.frag of the FractalShader sample, changed to be a background of the FramePacing sample. The iteration and
// the colors are the ones of that shader. What was changed: the point of a pixel comes from the place of the pixel on the screen
// and the zoom from a phase of the animation (it was a texture coordinate, with a scale and a offset the app worked out), and the
// number of iterations is given to the shader (it was a constant written into it).
//
// It is used as a adjustable GPU load: a zoom into the Mandelbrot set, as the FractalShader, GpuTimestamp and ShaderClock samples
// show one. It is the cheapest of the backgrounds, and the one a low end GPU can draw: a iteration is a handful of operations.
//
// The zoom is into Seahorse Valley, the neck between the main body of the set and the disc to the left of it, and it ends on a
// small copy of the set that sits in the spirals there (the FractalShader sample zooms into the small copy at -1.749 on the real
// axis, where a few iterations show all there is). The place was picked for what the load does to it:
// - It can be seen. The edge of the set is on screen all the way, and the closer the view is the more iterations a pixel next to
//   the edge needs before it leaves. With the default load the small copy is a black blob among black shapes, with four times
//   that it is a copy of the set among spirals, and more keeps adding detail.
// - It costs. A pixel inside the set takes every iteration, and something of the set is on screen at both ends of the zoom: the
//   main body before the zoom (about four tenths of the view) and the small copy at the end of it (about a quarter). In between
//   next to nothing is inside, and a high load costs less there.
//
// The view is 0.1 wide before the zoom and 64 times smaller at the end of it. That is about as far as the 32 bit float of a
// shader goes: at the end a pixel of a screen that is 1920 wide is fourteen of the steps a float has at this place, and not far
// beyond it the picture turns into blocks. (A OpenGL ES 2 device without highp in the fragment shader shows blocks long before.)
//
// The GPU load is the number of iterations a pixel can take, eight for each step of the load: the default load of 16 gives 128,
// which draws a third of the view at the end of the zoom (with 32 next to nothing of it is drawn). A pixel inside the set takes
// all of them, so the cost grows with the load where the set is on screen.
//
// The animation zooms in for half a period of the travel and out again, so it is the same picture when the period wraps.
//
// The animation is given as three phases in [0,1) and everything repeats when a phase wraps, so the time never loses precision.
//
// The GLES2, GLES3 and Vulkan samples each have their own copy of this shader, keep them the same.

precision highp float;

// x = the zoom, y = the sway, z = the slow change of shape and colour (not used here), w = the scene (not used here, the scene
// selects this shader)
uniform vec4 Phase;
// The size of the screen in pixels
uniform vec2 Resolution;
// The GPU load: a pixel can take eight times this many iterations
uniform float Steps;

out vec4 o_color;

#define PHASE Phase
#define RESOLUTION Resolution
#define STEPS Steps
#define FLIP_Y 1.0

const float TAU = 6.28318530718;
// As many iterations as the highest GPU load of the sample gives
const int MAX_ITERATIONS = 8192;
// The iterations a step of the GPU load is
const float ITERATIONS_PER_STEP = 8.0;
// How far the animation zooms in, in doublings (6 = 64 times), and the place it zooms in at: the middle of a small copy of the
// set (its period is 19 and it is 0.0005 of the size of the set)
const float ZOOM_DOUBLINGS = 6.0;
const vec2 ZOOM_CENTER = vec2(-0.73670985, 0.19024034);
// Half the width of the view before the zoom
const float VIEW_HALF_WIDTH = 0.0512;

void main()
{
  // The view is as wide as the screen, so its height follows the shape of the window
  vec2 p = (2.0 * gl_FragCoord.xy - RESOLUTION) / RESOLUTION.x;
  p.y *= FLIP_Y;
  float scale = exp2(-ZOOM_DOUBLINGS * 0.5 * (1.0 - cos(TAU * PHASE.x)));
  vec2 c = (p * VIEW_HALF_WIDTH * scale) + ZOOM_CENTER;

  int iterations = int(clamp(STEPS * ITERATIONS_PER_STEP, 1.0, float(MAX_ITERATIONS)));
  vec2 v = vec2(0.0);
  float count = float(iterations);
  float outside = 0.0;
  for (int i = 0; i < MAX_ITERATIONS; ++i)
  {
    if (i >= iterations)
    {
      break;
    }
    v = c + vec2((v.x * v.x) - (v.y * v.y), v.x * v.y * 2.0);
    if (dot(v, v) > 4.0)
    {
      count = float(i);
      outside = 1.0;
      break;
    }
  }

  // The set itself is black, what is outside it gets its color from how soon it left
  vec3 color = (0.5 + (0.5 * cos(3.0 + (count * 0.15) + vec3(0.0, 0.6, 1.0)))) * outside;
  o_color = vec4(color, 1.0);
}
