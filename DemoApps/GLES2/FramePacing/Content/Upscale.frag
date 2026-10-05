// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The background of the FramePacing sample drawn at a lower resolution is enlarged to the screen with this: the background was drawn
// into the lower left part of a texture (the upper left in Vulkan), and every pixel of the screen reads its place in that part.
//
// The GLES2, GLES3 and Vulkan samples each have their own copy of this shader, keep them the same.

#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

// xy = the part of the texture the background was drawn into, zw = the last place inside that part a sample may be taken at
uniform vec4 Phase;
// The size of the screen in pixels
uniform vec2 Resolution;
uniform sampler2D Texture;

void main()
{
  vec2 uv = min(gl_FragCoord.xy / Resolution * Phase.xy, Phase.zw);
  gl_FragColor = vec4(texture2D(Texture, uv).rgb, 1.0);
}
