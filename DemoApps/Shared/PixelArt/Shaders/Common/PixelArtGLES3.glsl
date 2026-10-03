// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The PixelArt prologue for OpenGL ES 3. The renderer puts the '#version' line in front of it, then the iChannel samplers of the pass
// (their type comes from Scene.json), the Common tab of the scene, its Params.glsl and finally the tab itself.
//
// It gives a tab what shadertoy gives it: the uniforms, a main() that calls mainImage(), and the PARAM() macro that reads the adjustable
// constants (the "Params" of Scene.json, in the order they are listed).

#define PIXELART 1
// Like shadertoy on a desktop GPU (shadertoy uses 0 on a mobile device), some shaders pick their quality with it
#define HW_PERFORMANCE 1

precision highp float;
precision highp int;
precision highp sampler2D;
precision highp samplerCube;

// The shadertoy uniforms (iResolution is the size of the pass, iMouse is given in the pixels of the pass)
uniform vec3 iResolution;
uniform float iTime;
uniform float iTimeDelta;
uniform float iFrameRate;
uniform int iFrame;
uniform vec4 iMouse;
uniform vec4 iDate;
uniform float iSampleRate;
uniform float iChannelTime[4];
uniform vec3 iChannelResolution[4];

// The adjustable constants: slot i is PixelArt_Params[i / 4][i % 4]
uniform vec4 PixelArt_Params[16];
// x = 1 for the image pass, y = what is done to the color the image pass writes (0 = nothing, 1 = linear to sRGB, 2 = sRGB to linear)
uniform vec4 PixelArt_Pass;

#define PARAM(i) PixelArt_Params[(i) / 4][(i) % 4]

out vec4 PixelArt_FragColor;

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
  // OpenGL ES has its origin in the bottom left corner, like shadertoy
  vec4 color = vec4(0.0, 0.0, 0.0, 1.0);
  mainImage(color, gl_FragCoord.xy);
  if (PixelArt_Pass.x > 0.5)
  {
    if (PixelArt_Pass.y > 1.5)
    {
      color.rgb = PixelArt_SrgbToLinear(color.rgb);
    }
    else if (PixelArt_Pass.y > 0.5)
    {
      color.rgb = PixelArt_LinearToSrgb(color.rgb);
    }
    // The screen is opaque, like on shadertoy
    color.a = 1.0;
  }
  PixelArt_FragColor = color;
}
