#version 300 es
// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The background of the FramePacing sample, used as a adjustable GPU load. It has two scenes: a flight through a fractal lattice, and
// a hall of columns that scrolls sideways at a constant speed (which makes a stutter easy to see).
//
// The lattice is a sphere inversion fractal (a "pseudo Kleinian", the fold and invert distance estimate Knighty described on
// fractalforums.com): space is tiled with cubes and every cube is turned inside out around its center, a few times over. The ray is
// sphere traced in a fixed number of steps without a early exit, so every pixel costs the same and the cost grows linearly with the
// number of steps. More steps reach further into the lattice and resolve finer detail.
//
// The hall is sphere traced the same way: rows of fluted columns on a mirroring floor, repeated along the way of the camera.
//
// The animation is given as three phases in [0,1) and everything repeats when a phase wraps, so the time never loses precision.
//
// The GLES2, GLES3 and Vulkan samples each have their own copy of this shader, keep them the same.

precision highp float;

// x = the travel of the camera, y = the sway of the camera, z = the colors and the shape of the lattice, w = the scene (0 = the flight,
// 1 = the hall)
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

// The flight through the lattice
vec3 RenderFlight(vec2 uv, float steps)
{
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
  return color;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// The hall: rows of fluted columns on a floor of polished tiles, at dusk. The camera only travels sideways, at a constant speed, so every
// column, every shadow and every tile crosses the screen at a constant speed: a frame that is shown too long or too short is easy to see.
// ---------------------------------------------------------------------------------------------------------------------------------------------

// The distance between two columns of a row, and between two rows
const float COLUMN_SPACING = 2.0;
const float ROW_SPACING = 2.6;
// How far the first row is from the camera, and the index of the last row
const float FIRST_ROW = 2.6;
const float LAST_ROW = 3.0;
const float COLUMN_RADIUS = 0.27;
const float COLUMN_HEIGHT = 2.5;
// The camera passes this many columns before the travel repeats (a whole number, so it repeats without a jump)
const float HALL_TRAVEL = 30.0;
const float HALL_FAR = 40.0;
// The steps of the shadow ray (the same for every pixel, they are not part of the GPU load that is set)
const int HALL_SHADOW_STEPS = 24;
// The low sun: the direction to it
const vec3 HALL_LIGHT = vec3(-0.7682, 0.3841, -0.5121);

float Box(vec3 p, vec3 halfSize)
{
  vec3 q = abs(p) - halfSize;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// The distance to the columns of one row and the beam they carry
float ColumnRow(vec3 p, float row)
{
  // Every other row is shifted half a spacing, so the rows do not hide each other
  float x = p.x + 0.5 * COLUMN_SPACING * mod(row, 2.0);
  vec3 q = vec3(mod(x + 0.5 * COLUMN_SPACING, COLUMN_SPACING) - 0.5 * COLUMN_SPACING, p.y, p.z - (FIRST_ROW + row * ROW_SPACING));
  // A fluted shaft
  float flutes = 0.5 + 0.5 * cos(16.0 * atan(q.z, q.x));
  float shaft = max(length(q.xz) - COLUMN_RADIUS * (1.0 - 0.07 * flutes), q.y - COLUMN_HEIGHT);
  // A plain ring ends the flutes at the foot and at the head of the shaft
  float ring = max(length(q.xz) - 1.12 * COLUMN_RADIUS, min(abs(q.y - 0.24), abs(q.y - (COLUMN_HEIGHT - 0.16))) - 0.06);
  shaft = min(shaft, ring);
  float base = Box(q - vec3(0.0, 0.09, 0.0), vec3(0.36, 0.09, 0.36));
  float capital = Box(q - vec3(0.0, COLUMN_HEIGHT, 0.0), vec3(0.36, 0.10, 0.36));
  vec2 b = abs(vec2(q.y - (COLUMN_HEIGHT + 0.36), q.z)) - vec2(0.26, 0.33);
  float beam = length(max(b, 0.0)) + min(max(b.x, b.y), 0.0);
  return min(min(shaft, beam), min(base, capital));
}

// The distance to the stone of the hall at p: the columns and the beams
float HallStone(vec3 p)
{
  // The two rows the point is between
  float rowA = clamp(floor((p.z - FIRST_ROW) / ROW_SPACING), 0.0, LAST_ROW);
  float rowB = min(rowA + 1.0, LAST_ROW);
  return min(ColumnRow(p, rowA), ColumnRow(p, rowB));
}

// The distance to the hall at p. material: 0 = the floor, 1 = stone
float Hall(vec3 p, out float material)
{
  float stone = HallStone(p);
  material = stone < p.y ? 1.0 : 0.0;
  return min(stone, p.y);
}

vec3 HallNormal(vec3 p)
{
  float unused;
  vec2 e = vec2(1.0, -1.0) * 0.0015;
  return normalize(e.xyy * Hall(p + e.xyy, unused) + e.yyx * Hall(p + e.yyx, unused) + e.yxy * Hall(p + e.yxy, unused) +
                   e.xxx * Hall(p + e.xxx, unused));
}

// The sky at dusk: a glow at the horizon under a deep blue
vec3 HallSky(vec3 direction)
{
  float height = clamp(direction.y, 0.0, 1.0);
  vec3 sky = mix(vec3(1.00, 0.40, 0.11), vec3(0.11, 0.06, 0.22), smoothstep(0.0, 0.07, height));
  return mix(sky, vec3(0.006, 0.014, 0.060), smoothstep(0.04, 0.40, height));
}

// How much of the light reaches the point (a soft shadow). The light comes from above, so only the stone can be in its way.
float HallShadow(vec3 position, vec3 lightDirection)
{
  float shade = 1.0;
  float t = 0.08;
  for (int i = 0; i < HALL_SHADOW_STEPS; ++i)
  {
    float d = HallStone(position + lightDirection * t);
    shade = min(shade, 10.0 * d / t);
    t += clamp(d, 0.04, 0.60);
  }
  return clamp(shade, 0.0, 1.0);
}

// The color of a point of the hall in the light of the low sun, without its reflection
vec3 HallShade(vec3 position, vec3 normal, float material, float shadow)
{
  vec3 lightDirection = HALL_LIGHT;
  float unused;
  // Where stone meets stone or the floor it is darker
  float occlusion = clamp(0.30 + 0.70 * Hall(position + normal * 0.22, unused) / 0.22, 0.0, 1.0);
  vec3 albedo = vec3(0.74, 0.68, 0.60);
  if (material < 0.5)
  {
    // Tiles of dark and light stone with a thin joint
    vec2 tile = floor(position.xz);
    vec2 joint = abs(fract(position.xz) - 0.5);
    albedo = mix(vec3(0.020, 0.024, 0.036), vec3(0.300, 0.280, 0.255), mod(tile.x + tile.y, 2.0));
    albedo *= 1.0 - 0.75 * smoothstep(0.482, 0.496, max(joint.x, joint.y));
  }
  float sun = max(dot(normal, lightDirection), 0.0) * shadow;
  // The sky lights what faces up, and the sunlit floor what faces down
  float sky = 0.5 + 0.5 * normal.y;
  vec3 ambient = vec3(0.045, 0.080, 0.210) * sky + vec3(0.110, 0.060, 0.030) * (1.0 - sky);
  return albedo * (vec3(3.80, 2.25, 1.05) * sun + ambient * occlusion);
}

vec3 RenderHall(vec2 uv, float steps)
{
  // The camera looks across the hall and travels along it
  vec3 origin = vec3(PHASE.x * HALL_TRAVEL * COLUMN_SPACING, 1.15, 0.0);
  vec3 direction = normalize(vec3(uv.x, uv.y + 0.12, 1.5));

  // Sphere trace in a fixed number of steps
  float t = 0.02;
  float d = 1.0;
  float material = 0.0;
  int stepCount = int(steps);
  for (int i = 0; i < stepCount; ++i)
  {
    d = Hall(origin + direction * t, material);
    t = min(t + 0.85 * d, HALL_FAR);
  }

  vec3 sky = HallSky(direction);
  vec3 position = origin + direction * t;
  vec3 normal = HallNormal(position);
  vec3 lightDirection = HALL_LIGHT;
  vec3 color = HallShade(position, normal, material, HallShadow(position + normal * 0.02, lightDirection));

  // The polished floor mirrors the hall: the mirrored ray is traced in a quarter of the steps
  float floorMask = 1.0 - material;
  vec3 mirrorDirection = reflect(direction, normal);
  float mirrorSteps = max(floor(0.25 * steps), 1.0);
  float mirrorT = 0.03;
  float mirrorD = 1.0;
  int mirrorStepCount = int(mirrorSteps);
  for (int j = 0; j < mirrorStepCount; ++j)
  {
    // The mirrored ray leaves the floor, so only the stone can be in its way
    mirrorD = HallStone(position + mirrorDirection * mirrorT);
    mirrorT = min(mirrorT + 0.85 * mirrorD, HALL_FAR);
  }
  vec3 mirrorPosition = position + mirrorDirection * mirrorT;
  vec3 mirrorSky = 0.50 * HallSky(mirrorDirection);
  vec3 mirror = HallShade(mirrorPosition, HallNormal(mirrorPosition), 1.0, 1.0);
  // A ray that did not get to the stone in its steps mirrors the sky
  mirror = mix(mirrorSky, mirror, exp(-0.055 * mirrorT) * (1.0 - smoothstep(0.01, 0.06, mirrorD)));
  float fresnel = pow(1.0 - clamp(dot(normal, -direction), 0.0, 1.0), 5.0);
  color = mix(color, color + mirror * (0.10 + 0.70 * fresnel), floorMask);

  // The sun glints on the stone
  float glint = pow(clamp(dot(reflect(direction, normal), lightDirection), 0.0, 1.0), 24.0);
  color += vec3(1.0, 0.72, 0.40) * glint * 0.35 * material;

  // The hall fades into the blue of the dusk, and a ray that left it shows the sky
  vec3 haze = mix(vec3(0.022, 0.032, 0.105), sky, 0.30);
  color = mix(haze, color, exp(-0.055 * t));
  // A ray that did not get to a surface in its steps ends in the haze: with few steps the far rows are lost in it
  color = mix(color, haze, smoothstep(0.02, 0.20, d));
  color = mix(color, sky, smoothstep(0.85 * HALL_FAR, HALL_FAR, t));

  // Tone map, then a vignette
  color *= 1.10;
  color = color / (1.0 + color);
  color = pow(color, vec3(0.4545));
  color = color * color * (3.0 - 2.0 * color);
  color *= 1.0 - 0.16 * dot(uv, uv);
  return color;
}

void main()
{
  vec2 uv = (2.0 * gl_FragCoord.xy - RESOLUTION) / RESOLUTION.y;
  uv.y *= FLIP_Y;
  float steps = clamp(STEPS, 1.0, float(MAX_STEPS));

  // The scene is the fourth value of the phases
  vec3 color;
  if (PHASE.w < 0.5)
  {
    color = RenderFlight(uv, steps);
  }
  else
  {
    color = RenderHall(uv, steps);
  }
  o_color = vec4(color, 1.0);
}
