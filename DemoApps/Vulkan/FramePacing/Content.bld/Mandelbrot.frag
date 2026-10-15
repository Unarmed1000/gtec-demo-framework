#version 450
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
// The zoom starts on the set as a whole and goes into Seahorse Valley, the neck between the main body of the set and the disc to
// the left of it, onto a small copy of the set that sits in the spirals there (the FractalShader sample zooms into the small copy
// at -1.749 on the real axis, where a few iterations show all there is). The place was picked for what the load does to it: the
// closer the view is to the neck the more iterations a pixel next to the edge needs before it leaves, so more load draws more of
// the spirals. With the default load of 16 the set is drawn before the zoom, and from about half way in the view is black (the
// iterations run out before a pixel leaves); with 64 the spirals are drawn all the way, and more keeps adding detail.
//
// The view is 3.2 wide before the zoom and 64 times smaller at the end of it, far from where the 32 bit float of a shader turns
// the picture into blocks. (A OpenGL ES 2 device without highp in the fragment shader shows blocks towards the end of the zoom.)
//
// The GPU load is the number of iterations a pixel can take. A pixel inside the set, or one that has not left when they run out,
// takes all of them: with the default load of 16 a pixel costs 14 iterations on average over the zoom, and from half way in
// nearly every pixel costs all 16.
//
// The animation zooms in for half a period of the travel and out again, so it is the same picture when the period wraps.
//
// The animation is given as three phases in [0,1) and everything repeats when a phase wraps, so the time never loses precision.
//
// The GLES2, GLES3 and Vulkan samples each have their own copy of this shader, keep them the same.

layout(push_constant) uniform PushConstants
{
  // x = the zoom, y = the sway, z = the slow change of shape and colour (not used here), w = the scene (not used here, the scene
  // selects this shader)
  vec4 Phase;
  // The size of the screen in pixels
  vec2 Resolution;
  // The GPU load: the iterations a pixel can take
  float Steps;
  float Reserved;
}
g_pushConstants;

layout(location = 0) out vec4 o_color;

#define PHASE g_pushConstants.Phase
#define RESOLUTION g_pushConstants.Resolution
#define STEPS g_pushConstants.Steps
// Vulkan has the origin at the top, flip it so the picture is the same as the OpenGL ES one
#define FLIP_Y -1.0

const float TAU = 6.28318530718;
// As many iterations as the highest GPU load of the sample gives
const int MAX_ITERATIONS = 1024;
// How far the animation zooms in, in doublings (6 = 64 times), and the place it zooms in at: the middle of a small copy of the
// set (its period is 19 and it is 0.0005 of the size of the set)
const float ZOOM_DOUBLINGS = 6.0;
const vec2 ZOOM_CENTER = vec2(-0.73670985, 0.19024034);
// Half the width of the view before the zoom
const float VIEW_HALF_WIDTH = 1.6;

void main()
{
  // The view is as wide as the screen, so its height follows the shape of the window
  vec2 p = (2.0 * gl_FragCoord.xy - RESOLUTION) / RESOLUTION.x;
  p.y *= FLIP_Y;
  float scale = exp2(-ZOOM_DOUBLINGS * 0.5 * (1.0 - cos(TAU * PHASE.x)));
  vec2 c = (p * VIEW_HALF_WIDTH * scale) + ZOOM_CENTER;

  int iterations = int(clamp(STEPS, 1.0, float(MAX_ITERATIONS)));
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
