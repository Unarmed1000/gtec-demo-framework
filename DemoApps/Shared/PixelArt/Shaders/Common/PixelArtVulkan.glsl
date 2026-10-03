// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The PixelArt prologue for Vulkan. Every tab of a scene has a small .frag in the Vulkan app that includes this, declares the iChannel
// samplers of the pass (binding 1 + N for iChannelN, the type comes from Scene.json) and includes the scene's Params.glsl and the tab.
//
// It gives a tab what shadertoy gives it: the uniforms, a main() that calls mainImage(), and the PARAM() macro that reads the adjustable
// constants (the "Params" of Scene.json, in the order they are listed).

#define PIXELART 1
// Like shadertoy on a desktop GPU (shadertoy uses 0 on a mobile device), some shaders pick their quality with it
#define HW_PERFORMANCE 1

// What is the same for every pass of a frame
layout(std140, set = 0, binding = 0) uniform PixelArtFrameBlock
{
  // x = iTime, y = iTimeDelta, z = iFrameRate, w = iFrame
  vec4 Time;
  vec4 Date;
  vec4 ChannelTime;
  // x = iSampleRate
  vec4 Misc;
  // The adjustable constants: slot i is Params[i / 4][i % 4]
  vec4 Params[16];
}
PixelArt_Frame;

// What is different for every pass
layout(push_constant) uniform PixelArtPassBlock
{
  // xyz = iResolution
  vec4 Resolution;
  vec4 Mouse;
  // x = 1 for the image pass, y = what is done to the color the image pass writes (0 = nothing, 1 = linear to sRGB, 2 = sRGB to linear)
  vec4 Pass;
  vec4 ChannelResolution[4];
}
PixelArt_Pass;

#define iResolution PixelArt_Pass.Resolution.xyz
#define iMouse PixelArt_Pass.Mouse
#define iChannelResolution PixelArt_Pass.ChannelResolution
#define iTime PixelArt_Frame.Time.x
#define iTimeDelta PixelArt_Frame.Time.y
#define iFrameRate PixelArt_Frame.Time.z
#define iFrame int(PixelArt_Frame.Time.w)
#define iDate PixelArt_Frame.Date
#define iChannelTime PixelArt_Frame.ChannelTime
#define iSampleRate PixelArt_Frame.Misc.x

#define PARAM(i) PixelArt_Frame.Params[(i) / 4][(i) % 4]

layout(location = 0) out vec4 PixelArt_FragColor;

void mainImage(out vec4 fragColor, in vec2 fragCoord);

vec3 PixelArt_LinearToSrgb(vec3 color)
{
  color = clamp(color, 0.0, 1.0);
  return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), color));
}

vec3 PixelArt_SrgbToLinear(vec3 color)
{
  color = clamp(color, 0.0, 1.0);
  return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), color));
}

void main()
{
  vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
  vec2 fragCoord = gl_FragCoord.xy;
  if (PixelArt_Pass.Pass.x > 0.5)
  {
    // Vulkan has its origin in the top left corner. Only the image pass is flipped to shadertoy's bottom left origin: a buffer is written
    // and read with the same origin, so it is stored upside down and the image pass shows it the right way up.
    fragCoord.y = PixelArt_Pass.Resolution.y - fragCoord.y;
  }
  mainImage(color, fragCoord);
  if (PixelArt_Pass.Pass.x > 0.5)
  {
    if (PixelArt_Pass.Pass.y > 1.5)
    {
      color.rgb = PixelArt_SrgbToLinear(color.rgb);
    }
    else if (PixelArt_Pass.Pass.y > 0.5)
    {
      color.rgb = PixelArt_LinearToSrgb(color.rgb);
    }
    // The screen is opaque, like on shadertoy
    color.a = 1.0;
  }
  PixelArt_FragColor = color;
}
