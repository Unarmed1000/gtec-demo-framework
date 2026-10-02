#version 300 es
// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The background of the FramePacing sample: a flight through a fractal lattice, used as a adjustable GPU load.
//
// The lattice is a sphere inversion fractal (a "pseudo Kleinian", the fold and invert distance estimate Knighty described on
// fractalforums.com): space is tiled with cubes and every cube is turned inside out around its center, a few times over. The ray is
// sphere traced in a fixed number of steps without a early exit, so every pixel costs the same and the cost grows linearly with the
// number of steps. More steps reach further into the lattice and resolve finer detail.
//
// The animation is given as three phases in [0,1) and everything repeats when a phase wraps, so the time never loses precision.
//
// The GLES2, GLES3 and Vulkan samples each have their own copy of this shader, keep them the same.

precision highp float;

// x = the flight through the lattice, y = the sway of the camera, z = the colors and the shape of the lattice
uniform vec4 Phase;
// The size of the screen in pixels
uniform vec2 Resolution;
// The number of steps the ray is marched in
uniform float Steps;

out vec4 o_color;

#define PHASE Phase
#define RESOLUTION Resolution
#define STEPS Steps
#define FLIP_Y 1.0

const float TAU = 6.28318530718;
const int MAX_STEPS = 1024;
// The number of times the lattice is folded and inverted
const int FOLD_COUNT = 7;
// The lattice repeats every 2 units, the camera flies this far before the flight repeats
const float FLIGHT_LENGTH = 6.0;
// How far the ray is marched at most
const float FAR = 7.0;

mat2 Rotate(float angle)
{
  float c = cos(angle);
  float s = sin(angle);
  return mat2(c, s, -s, c);
}

vec3 Palette(float t)
{
  return 0.5 + 0.5 * cos(TAU * (t + vec3(0.0, 0.33, 0.67)));
}

// The distance to the lattice at p (a estimate that never overshoots).
// inversion: the radius the cubes are turned inside out by, it shapes the lattice.
// trap: the closest the point came to the axes of the cubes (xyz) and to their centers (w) while it was folded, used to color the lattice
float Lattice(vec3 p, float inversion, out vec4 trap)
{
  float scale = 1.0;
  trap = vec4(100.0);
  for (int i = 0; i < FOLD_COUNT; ++i)
  {
    // Tile space with cubes of size 2 and continue in the cube the point is in
    p = 2.0 * fract(0.5 * p + 0.5) - 1.0;
    // Turn the cube inside out around its center
    float r2 = dot(p, p);
    trap = min(trap, vec4(abs(p), r2));
    float k = inversion / r2;
    p *= k;
    scale *= k;
  }
  // What is left is measured against the floor of the cube and scaled back
  return 0.25 * abs(p.y) / scale;
}

// Polished stone: gold where the folds are deep, a dark teal in the open
vec3 Stone(float depth)
{
  return mix(vec3(0.95, 0.52, 0.14), vec3(0.03, 0.22, 0.30), smoothstep(0.20, 0.80, depth));
}

// The deepest folds glow, in waves that run through the lattice
vec3 Ember(vec3 position, float depth)
{
  float pulse = 0.60 + 0.40 * sin(TAU * (0.5 * position.z + 0.5 * position.x - 3.0 * PHASE.y));
  return vec3(1.0, 0.30, 0.05) * (pow(1.0 - depth, 8.0) * 2.4 * pulse);
}

void main()
{
  vec2 uv = (2.0 * gl_FragCoord.xy - RESOLUTION) / RESOLUTION.y;
  uv.y *= FLIP_Y;

  // The lattice slowly changes its shape
  float inversion = 1.18 + 0.10 * sin(TAU * PHASE.z);

  // The camera flies through the lattice and sways a little
  float z = PHASE.x * FLIGHT_LENGTH;
  float sway = TAU * PHASE.y;
  vec3 origin = vec3(1.0 + 0.12 * sin(sway), 0.42 + 0.08 * cos(sway), z);
  vec3 forward = normalize(vec3(0.25 * sin(sway + 1.0), 0.10 * sin(2.0 * sway), 1.0));
  vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
  vec3 up = cross(forward, right);
  vec3 direction = normalize(uv.x * right + uv.y * up + 1.5 * forward);

  float steps = clamp(STEPS, 1.0, float(MAX_STEPS));

  // Sphere trace: every step moves the distance the lattice is away, so the ray closes in on the surface and stays there
  float t = 0.002;
  vec4 trap = vec4(0.0);
  int stepCount = int(steps);
  for (int i = 0; i < stepCount; ++i)
  {
    float d = Lattice(origin + direction * t, inversion, trap);
    t = min(t + 0.9 * d, FAR);
  }

  // The surface the ray ended at. The surfaces of the lattice are shells without a thickness, so the distance to them has a crease
  // on the surface: the normal is taken a little in front of it, where all four samples are on the side the ray came from.
  vec3 position = origin + direction * t;
  vec4 unusedTrap;
  vec2 e = vec2(1.0, -1.0) * (0.0005 + 0.0010 * t);
  vec3 front = position - direction * (3.0 * e.x);
  vec3 normal = normalize(e.xyy * Lattice(front + e.xyy, inversion, unusedTrap) + e.yyx * Lattice(front + e.yyx, inversion, unusedTrap) +
                          e.yxy * Lattice(front + e.yxy, inversion, unusedTrap) + e.xxx * Lattice(front + e.xxx, inversion, unusedTrap));

  // A shell is seen from both sides, so the normal has to face the ray
  normal = dot(normal, direction) > 0.0 ? -normal : normal;

  // The floors of the lattice are water: small waves tilt their normal
  float water = smoothstep(0.990, 0.999, normal.y);
  float waveTime = TAU * 5.0 * PHASE.y;
  vec2 wave = vec2(sin(23.0 * position.x + 9.0 * position.z + waveTime) + sin(41.0 * position.z - 17.0 * position.x - 2.0 * waveTime),
                   sin(29.0 * position.z - 7.0 * position.x - waveTime) + sin(37.0 * position.x + 13.0 * position.z + 2.0 * waveTime));
  normal = normalize(normal + vec3(wave.x, 0.0, wave.y) * (0.018 * water));

  // The deeper a point sits in the folds the darker it is
  float depth = clamp(1.5 * trap.w, 0.0, 1.0);
  float occlusion = depth * depth * (3.0 - 2.0 * depth);

  vec3 albedo = mix(Stone(depth), Palette(0.55 + 0.6 * trap.y + PHASE.z), 0.18);
  albedo = mix(albedo, vec3(0.010, 0.060, 0.080), water);

  // A warm lamp that travels with the camera, a cool light from above and the sky in the grazing angles
  vec3 lampDirection = normalize(origin + up * 0.25 + right * 0.20 - position);
  float lamp = clamp(dot(normal, lampDirection), 0.0, 1.0) / (1.0 + 1.2 * t * t);
  float sky = 0.5 + 0.5 * normal.y;
  float facing = clamp(dot(normal, -direction), 0.0, 1.0);
  float fresnel = pow(1.0 - facing, 5.0);
  float specular = pow(clamp(dot(reflect(direction, normal), lampDirection), 0.0, 1.0), mix(40.0, 160.0, water)) / (1.0 + 1.2 * t * t);

  vec3 color = albedo * (vec3(2.60, 2.00, 1.40) * lamp + vec3(0.10, 0.20, 0.36) * sky) * occlusion;
  color += vec3(1.0, 0.85, 0.6) * specular * mix(2.5, 9.0, water) * occlusion;
  color += vec3(0.20, 0.40, 0.80) * fresnel * mix(0.6, 1.6, water) * occlusion;
  color += Ember(position, depth) * (1.0 - water);

  // What the water mirrors. The lattice below the water is the mirror image of the lattice above it, so the mirrored ray is traced
  // there, in a quarter of the steps and shaded by the folds alone. It starts a little away from the water, as a ray that grazes it
  // would spend its steps there.
  vec3 mirrorDirection = reflect(direction, normal);
  float mirrorSteps = max(floor(0.25 * steps), 1.0);
  float mirrorT = 0.08 / max(mirrorDirection.y, 0.08);
  float mirrorD = 1.0;
  vec4 mirrorTrap = vec4(0.0);
  int mirrorStepCount = int(mirrorSteps);
  for (int j = 0; j < mirrorStepCount; ++j)
  {
    mirrorD = Lattice(position + mirrorDirection * mirrorT, inversion, mirrorTrap);
    mirrorT = min(mirrorT + 0.9 * mirrorD, FAR);
  }
  vec3 fogColor = mix(vec3(0.008, 0.016, 0.040), vec3(0.050, 0.024, 0.075), 0.5 + 0.5 * uv.y);
  float mirrorDepth = clamp(1.5 * mirrorTrap.w, 0.0, 1.0);
  vec3 mirrorPosition = position + mirrorDirection * mirrorT;
  vec3 mirror = Stone(mirrorDepth) * (0.10 + 0.55 * mirrorDepth * mirrorDepth) + Ember(mirrorPosition, mirrorDepth);
  // A ray that did not get close to the lattice in its steps mirrors the dark
  mirror = mix(fogColor, mirror, (1.0 - smoothstep(0.004, 0.04, mirrorD)) * exp(-0.30 * mirrorT));
  color = mix(color, 0.35 * color + mirror * (0.45 + 0.55 * fresnel), water);

  // The lattice fades into the dark with the distance
  color = mix(fogColor, color, exp(-0.30 * t));

  // Tone map, then a vignette
  color *= 1.4;
  color = color / (1.0 + color);
  color = pow(color, vec3(0.4545));
  color = color * color * (3.0 - 2.0 * color);
  color *= 1.0 - 0.20 * dot(uv, uv);
  o_color = vec4(color, 1.0);
}
