// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// A background of the FramePacing sample, used as a adjustable GPU load: a lace of circles packed into circles, drawn with gold
// threads.
//
// The plane is cut into squares of two by two, and every square is turned inside out around the circle in its middle. Doing that a
// few times packs circles into circles, and every round more packs finer circles into the ones that are there. The picture is the
// same after two units sideways, so the travel wraps without a jump.
//
// The GPU load adds detail: every doubling of it is one more round (four rounds at a load of 16, ten at 1024), and what is left of
// the load is the number of samples a pixel is drawn with, which smooths the edges. So the cost grows linearly with the load. A
// round past the first four only shows where its circles are large enough on the screen, as finer ones would be noise. One round
// of one sample costs about a dozen operations, which makes the lowest load far cheaper than one step of the raymarched scenes in
// Raymarch.frag.
//
// The animation zooms into the lace and out again while the lace travels sideways, once per period of the travel. The fine
// middle of a flower opens into a web of threads on the way in, and the rounds a higher load adds come into view.
//
// The animation is given as three phases in [0,1) and everything repeats when a phase wraps, so the time never loses precision.
//
// The GLES2, GLES3 and Vulkan samples each have their own copy of this shader, keep them the same.

#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

// x = the travel sideways and the zoom, y = the sway, z = the slow change of shape and colour, w = the scene (not used here, the
// scene selects this shader)
uniform vec4 Phase;
// The size of the screen in pixels
uniform vec2 Resolution;
// The GPU load: the rounds of detail and the samples per pixel come from it
uniform float Steps;

#define PHASE Phase
#define RESOLUTION Resolution
#define STEPS Steps
#define FLIP_Y 1.0
#define o_color gl_FragColor

const float TAU = 6.28318530718;
// As many samples as the GPU load of the sample goes to
const int MAX_SAMPLES = 1024;
// The rounds of turning the squares inside out: the most there are, and the first ones, which always show in full
const int MAX_ROUNDS = 10;
const int BASE_ROUNDS = 4;
// A round past the first ones shows where the square it turns inside out is larger than this many pixels on the screen, and fades
// in over that range. A whole flower of circles fades that way, not its fine middle before its rim.
const float DETAIL_FADE_BEGIN = 8.0;
const float DETAIL_FADE_END = 40.0;
// How far the animation zooms in, in doublings (6 = 64 times), and the place of the lace it zooms in at
const float ZOOM_DOUBLINGS = 6.0;
const vec2 ZOOM_CENTER = vec2(0.38, 0.27);

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

// The colour of the lace at a place of the screen. 'pixel' is the size of a pixel in the units of the lace, so the threads are as
// fine at every size of the window and at every zoom.
vec3 Lace(vec2 screen, float pixel, float zoom, int rounds)
{
  vec2 p = Rotate(screen, 0.12 * sin(TAU * PHASE.y));
  p *= 0.9 + 0.08 * sin(TAU * PHASE.y + 1.0);
  // The lace travels sideways by four units of the screen a period, at every zoom. Where the period wraps the zoom is one and four
  // units are two squares of the lace, so there is no jump. In the middle of the period the view is at the place it zooms in at.
  p += vec2(4.0 * (PHASE.x - 0.5), 0.35 * sin(TAU * PHASE.z));
  p = ZOOM_CENTER + p / zoom;
  // The slow change of the shape is that many times smaller when enlarged, so it is as fast on the screen at every zoom
  float k = 1.16 + 0.10 * sin(TAU * PHASE.z) / zoom;

  float scale = 1.0;
  // How close the place came to the middle of a circle
  float trapCircle = 1e9;
  // How much the place is on a thread, and in the glow of one
  float line = 0.0;
  float glow = 0.0;
  for (int i = 0; i < MAX_ROUNDS; ++i)
  {
    if (i >= rounds)
    {
      break;
    }
    p = -1.0 + 2.0 * fract(0.5 * p + 0.5);
    // How much this round shows here: the square it turns inside out is one over the scale so far in size
    float show = (i < BASE_ROUNDS) ? 1.0 : smoothstep(DETAIL_FADE_BEGIN, DETAIL_FADE_END, 1.0 / (scale * pixel));
    float r2 = max(dot(p, p), 1e-4);
    float f = k / r2;
    p *= f;
    scale *= f;
    trapCircle = min(trapCircle, r2 / max(show, 1e-3));
    // How far the place is from the threads of this round, in pixels
    float thread = min(abs(p.x), abs(p.y)) / (scale * pixel);
    line = max(line, show * (1.0 - smoothstep(0.6, 1.8, thread)));
    glow = max(glow, show * exp(-0.06 * thread));
  }

  // The colours go once through the palette per period of the phase, so there is no jump of colour where the phase wraps
  float t = 0.35 * log2(1.0 + 6.0 * trapCircle) + PHASE.z + 0.04 * float(BASE_ROUNDS);
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
  // The zoom of the animation: in for half a period of the travel and out again, one where the period wraps
  float zoom = exp2(ZOOM_DOUBLINGS * 0.5 * (1.0 - cos(TAU * PHASE.x)));
  float screenPixel = 2.0 / RESOLUTION.y;
  float pixel = screenPixel / zoom;

  // Every doubling of the load is a round of detail, the rest of the load is samples
  float steps = max(STEPS, 1.0);
  int rounds = int(clamp(floor(log2(steps)), 1.0, float(MAX_ROUNDS)));
  int sampleCount = int(steps) / rounds;

  // The samples are spread evenly over the pixel (a sequence that fills a square evenly however many of it are taken). The first
  // one is the middle of the pixel.
  vec3 sum = vec3(0.0);
  float samples = 0.0;
  for (int i = 0; i < MAX_SAMPLES; ++i)
  {
    if (i >= sampleCount && i > 0)
    {
      break;
    }
    vec2 inPixel = fract(vec2(0.5) + float(i) * vec2(0.7548776662, 0.5698402910)) - 0.5;
    sum += Lace(p + inPixel * screenPixel, pixel, zoom, rounds);
    samples += 1.0;
  }
  vec3 color = sum / max(samples, 1.0);

  // A soft vignette keeps the UI on top readable
  float vignette = 1.0 - 0.28 * dot(p * vec2(0.55, 0.8), p * vec2(0.55, 0.8));
  o_color = vec4(color * clamp(vignette, 0.0, 1.0), 1.0);
}
