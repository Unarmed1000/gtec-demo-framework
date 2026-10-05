#version 450
// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// A background of the FramePacing sample, used as a adjustable GPU load: a lace of circles packed into circles, drawn with gold
// threads.
//
// The plane is cut into squares of two by two, and every square is turned inside out around the circle in its middle. Doing that a
// few times packs circles into circles. The picture is the same after two units sideways, so the travel wraps without a jump.
//
// The picture always has the same four rounds of that. The GPU load is the number of samples a pixel is drawn with: every sample is
// the whole picture at a slightly different place inside the pixel, so the cost grows linearly with the load and more load gives
// smoother edges. One sample costs a few dozen operations, which makes the lowest load far cheaper than one step of the raymarched
// scenes in Raymarch.frag.
//
// The animation is given as three phases in [0,1) and everything repeats when a phase wraps, so the time never loses precision.
//
// The GLES2, GLES3 and Vulkan samples each have their own copy of this shader, keep them the same.

layout(push_constant) uniform PushConstants
{
  // x = the travel sideways, y = the sway, z = the slow change of shape and colour, w = the scene (not used here, the scene selects
  // this shader)
  vec4 Phase;
  // The size of the screen in pixels
  vec2 Resolution;
  // The number of samples a pixel is drawn with
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
// As many samples as the GPU load of the sample goes to
const int MAX_SAMPLES = 1024;
// The rounds of turning the squares inside out. The look of the picture is these four.
const int ROUNDS = 4;

vec3 Palette(float t, vec3 a, vec3 b, vec3 c, vec3 d)
{
  return a + b * cos(TAU * (c * t + d));
}

vec2 Rotate(vec2 p, float angle)
{
  float s = sin(angle);
  float c = cos(angle);
  return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

// The colour of the lace at a place. 'pixel' is the size of a pixel there, so the threads are as fine at every size of the window.
vec3 Lace(vec2 p, float pixel)
{
  p = Rotate(p, 0.12 * sin(TAU * PHASE.y));
  p *= 0.9 + 0.08 * sin(TAU * PHASE.y + 1.0);
  p.x += 4.0 * PHASE.x;
  p.y += 0.35 * sin(TAU * PHASE.z);

  float k = 1.16 + 0.10 * sin(TAU * PHASE.z);
  float scale = 1.0;
  // How close the place came to the middle of a circle, and to a thread
  float trapCircle = 1e9;
  float trapAxis = 1e9;
  for (int i = 0; i < ROUNDS; ++i)
  {
    p = -1.0 + 2.0 * fract(0.5 * p + 0.5);
    float r2 = max(dot(p, p), 1e-4);
    float f = k / r2;
    p *= f;
    scale *= f;
    trapCircle = min(trapCircle, r2);
    trapAxis = min(trapAxis, min(abs(p.x), abs(p.y)) / scale);
  }
  // How far the place is from the threads, in pixels
  float thread = trapAxis / pixel;
  float line = 1.0 - smoothstep(0.6, 1.8, thread);
  float glow = exp(-0.06 * thread);

  float t = 0.35 * log2(1.0 + 6.0 * trapCircle) + 0.55 * PHASE.z + 0.04 * float(ROUNDS);
  vec3 body = Palette(t, vec3(0.20, 0.16, 0.26), vec3(0.22, 0.20, 0.30), vec3(1.0, 1.0, 1.0), vec3(0.62, 0.45, 0.30));
  body *= 0.35 + 0.65 * smoothstep(0.0, 0.6, sqrt(trapCircle));
  vec3 gold = vec3(1.00, 0.78, 0.42);
  vec3 color = mix(body, gold, line);
  color += 0.22 * gold * glow;
  return color;
}

void main()
{
  vec2 p = (2.0 * gl_FragCoord.xy - RESOLUTION) / RESOLUTION.y;
  p.y *= FLIP_Y;
  float pixel = 2.0 / RESOLUTION.y;

  // The samples are spread evenly over the pixel (a sequence that fills a square evenly however many of it are taken). The first
  // one is the middle of the pixel.
  vec3 sum = vec3(0.0);
  float samples = 0.0;
  for (int i = 0; i < MAX_SAMPLES; ++i)
  {
    if (float(i) >= STEPS)
    {
      break;
    }
    vec2 inPixel = fract(vec2(0.5) + float(i) * vec2(0.7548776662, 0.5698402910)) - 0.5;
    sum += Lace(p + inPixel * pixel, pixel);
    samples += 1.0;
  }
  vec3 color = sum / max(samples, 1.0);

  // A soft vignette keeps the UI on top readable
  float vignette = 1.0 - 0.28 * dot(p * vec2(0.55, 0.8), p * vec2(0.55, 0.8));
  o_color = vec4(color * clamp(vignette, 0.0, 1.0), 1.0);
}
