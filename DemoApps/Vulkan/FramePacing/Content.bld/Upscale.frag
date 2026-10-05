#version 450
// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The background of the FramePacing sample drawn at a lower resolution is enlarged to the screen with this: the background was drawn
// into the lower left part of a texture (the upper left in Vulkan), and every pixel of the screen reads its place in that part.
//
// The GLES2, GLES3 and Vulkan samples each have their own copy of this shader, keep them the same.

layout(push_constant) uniform PushConstants
{
  // xy = the part of the texture the background was drawn into, zw = the last place inside that part a sample may be taken at
  vec4 Phase;
  // The size of the screen in pixels
  vec2 Resolution;
  float Steps;
  float Reserved;
}
g_pushConstants;

layout(set = 0, binding = 0) uniform sampler2D g_texture;

layout(location = 0) out vec4 o_color;

void main()
{
  vec2 uv = min(gl_FragCoord.xy / g_pushConstants.Resolution * g_pushConstants.Phase.xy, g_pushConstants.Phase.zw);
  o_color = vec4(texture(g_texture, uv).rgb, 1.0);
}
