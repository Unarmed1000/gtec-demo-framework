#version 450
// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The background of the FramePacing sample: a flight through a tunnel of neon rings and glowing wires, used as a adjustable GPU load.
//
// The wires are the curves where two gyroid surfaces meet. The ray is marched in a fixed number of equal steps and every step adds the
// glow of the rings and wires near it (a volume, there is no surface to hit). So every pixel costs the same, the cost grows linearly
// with the number of steps, and the picture stays the same: more steps only give less noise.
//
// The animation is given as three phases in [0,1) and everything repeats when a phase wraps, so the time never loses precision.
//
// The GLES2, GLES3 and Vulkan samples each have their own copy of this shader, keep them the same.

layout(push_constant) uniform PushConstants
{
  // x = the flight through the tunnel, y = the roll of the camera, z = the colors
  vec4 Phase;
  // The size of the screen in pixels
  vec2 Resolution;
  // The number of steps the ray is marched in
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
// The length of the tunnel (8 pi): everything along it repeats with this period
const float TUNNEL_LENGTH = 25.13274123;
// The distance between two rings (16 rings in the tunnel)
const float RING_SPACING = 1.57079633;
const float RING_RADIUS = 1.2;
// How far the ray is marched
const float FAR = 10.0;
const int MAX_STEPS = 1024;

mat2 Rotate(float angle)
{
  float c = cos(angle);
  float s = sin(angle);
  return mat2(c, s, -s, c);
}

// The line the tunnel follows
vec2 TunnelCenter(float z)
{
  return vec2(1.5 * sin(z * 0.25), 1.0 * cos(z * 0.5));
}

float Gyroid(vec3 p)
{
  return dot(sin(p), cos(p.yzx));
}

vec3 Palette(float t)
{
  return 0.5 + 0.5 * cos(TAU * (t + vec3(0.0, 0.33, 0.67)));
}

// The light of the rings and the wires at p
vec3 Glow(vec3 p)
{
  vec3 q = p;
  q.xy -= TunnelCenter(p.z);
  float radius = length(q.xy);
  float colorPhase = PHASE.z;

  // The rings the camera flies through
  float ringZ = (fract(p.z / RING_SPACING + 0.5) - 0.5) * RING_SPACING;
  float ring = length(vec2(radius - RING_RADIUS, ringZ));
  vec3 light = Palette(p.z / TUNNEL_LENGTH + colorPhase) * (0.0140 / (ring * ring + 0.0012));

  // The wires around the tunnel: two gyroid surfaces, a wire runs where both are zero. The lattice twists along the tunnel and the
  // tunnel itself is kept clear.
  q.xy = Rotate(p.z * 0.25) * q.xy;
  float g1 = Gyroid(q * 1.4) / 1.4;
  float g2 = Gyroid(q.zxy * 2.2 + 1.7) / 2.2;
  float wire = length(vec2(g1, g2));
  float clearing = smoothstep(RING_RADIUS, RING_RADIUS + 0.7, radius);
  light += Palette(p.z * (2.0 / TUNNEL_LENGTH) + g2 * 0.5 + colorPhase + 0.45) * (clearing * 0.0130 / (wire * wire + 0.0035));
  return light;
}

float Hash(vec2 p)
{
  return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
  vec2 uv = (2.0 * gl_FragCoord.xy - RESOLUTION) / RESOLUTION.y;
  uv.y *= FLIP_Y;

  // The camera flies along the center line of the tunnel and rolls a little
  float z = PHASE.x * TUNNEL_LENGTH;
  vec3 origin = vec3(TunnelCenter(z), z);
  vec3 target = vec3(TunnelCenter(z + 1.5), z + 1.5);
  vec3 forward = normalize(target - origin);
  vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
  vec3 up = cross(forward, right);
  vec2 screen = Rotate(0.35 * sin(TAU * PHASE.y)) * uv;
  vec3 direction = normalize(screen.x * right + screen.y * up + 1.3 * forward);

  float steps = clamp(STEPS, 1.0, float(MAX_STEPS));
  float stepLength = FAR / steps;
  // Every pixel starts at its own offset into the first step, which turns the bands of the steps into noise
  float t = stepLength * Hash(gl_FragCoord.xy);

  vec3 sum = vec3(0.0);
  int stepCount = int(steps);
  for (int i = 0; i < stepCount; ++i)
  {
    // The light fades into the dark with the distance
    sum += Glow(origin + direction * t) * (exp(-0.26 * t) * stepLength);
    t += stepLength;
  }

  // Tone map, then a vignette
  vec3 color = 1.0 - exp(-2.2 * sum);
  color = pow(color, vec3(0.8)) + vec3(0.004, 0.006, 0.016);
  color *= 1.0 - 0.18 * dot(uv, uv);
  o_color = vec4(color, 1.0);
}
