#version 450
// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// A background of the FramePacing sample, used as a adjustable GPU load: a flight through a field of blobs that melt into each other.
//
// Space is cut into cells and every cell has three balls that circle its middle and are joined with a smooth minimum. The balls stay
// inside their cell, so a ray only ever has to know the balls of the cell it is in. The ray is sphere traced in a fixed number of
// steps without a early exit, so every pixel costs the same and the cost grows linearly with the number of steps. More steps reach
// further into the field. There are no shadow or mirror rays, and a step is a few additions and three square roots, which makes one
// step far cheaper than a step of the raymarched scenes in Raymarch.frag.
//
// The animation is given as three phases in [0,1) and everything repeats when a phase wraps, so the time never loses precision.
//
// The GLES2, GLES3 and Vulkan samples each have their own copy of this shader, keep them the same.

layout(push_constant) uniform PushConstants
{
  // x = the travel of the camera, y = the sway of the camera, z = the circling of the balls, w = the scene (not used here, the scene
  // selects this shader)
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
// As many steps as the GPU load of the sample goes to
const int MAX_STEPS = 1024;
// The size of a cell of the field
const vec3 CELL = vec3(4.4, 4.8, 4.4);
// The flight goes this many cells per round of the travel, and the field repeats after as many
const float ROWS = 8.0;

vec2 Rotate(vec2 p, float angle)
{
  float s = sin(angle);
  float c = cos(angle);
  return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float Hash21(vec2 p)
{
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

// The smaller of two distances, rounded off where they are close: it joins two shapes with a soft neck
float SmoothMin(float a, float b, float k)
{
  float h = max(k - abs(a - b), 0.0) / k;
  return min(a, b) - 0.25 * h * h * k;
}

// The distance to the blobs. Every other column of cells is half a cell higher. What is returned is never further than the wall of
// the cell, so a ray can not jump over the blobs of the next cell; the distance to the blobs themselves comes back in 'blob', and the
// cell in 'id'. All three balls get their place from one sine and one cosine.
float Field(vec3 p, out float blob, out vec3 id)
{
  vec2 column = floor(p.xz / CELL.xz + 0.5);
  p.y += 0.5 * CELL.y * mod(column.x + column.y, 2.0);
  id = vec3(column.x, floor(p.y / CELL.y + 0.5), column.y);
  vec3 q = p - CELL * id;
  vec3 wall = 0.5 * CELL - abs(q);
  float angle = TAU * (PHASE.z + 3.0 * id.z / ROWS) + 1.7 * id.x + 1.3 * id.y;
  float s = sin(angle);
  float c = cos(angle);
  float s2 = 2.0 * s * c;
  float c2 = c * c - s * s;
  float d = length(q - vec3(0.78 * s, 0.50 * c2, 0.78 * c)) - 0.66;
  d = SmoothMin(d, length(q - vec3(-0.70 * c, 0.74 * s, 0.62 * s2)) - 0.54, 0.62);
  d = SmoothMin(d, length(q - vec3(0.58 * c2, -0.72 * c, -0.66 * s)) - 0.47, 0.62);
  blob = d;
  return min(d, min(wall.x, min(wall.y, wall.z)) + 0.08);
}

// What is behind the blobs: a dark sky with two bands of light and stars that twinkle
vec3 Sky(vec3 direction)
{
  float up = direction.y;
  vec3 color = mix(vec3(0.020, 0.015, 0.080), vec3(0.150, 0.050, 0.300), smoothstep(-0.7, 0.6, up));
  color += vec3(0.550, 0.180, 0.600) * 0.30 * exp(-7.0 * abs(up + 0.04));
  color += vec3(0.100, 0.350, 0.800) * 0.18 * exp(-3.0 * abs(up - 0.45));
  vec2 g = direction.xy / max(abs(direction.z), 0.2) * 46.0;
  float h = Hash21(floor(g));
  float star = step(0.975, h) * smoothstep(0.20, 0.0, length(fract(g) - 0.5));
  return color + vec3(0.900, 0.900, 1.000) * star * (0.5 + 0.5 * sin(TAU * (2.0 * PHASE.y + 7.0 * h)));
}

void main()
{
  vec2 p = (2.0 * gl_FragCoord.xy - RESOLUTION) / RESOLUTION.y;
  p.y *= FLIP_Y;

  // The camera flies down a lane between the columns, a whole number of cells per round of the travel, and looks around a little
  vec3 origin = vec3(0.5 * CELL.x + 0.30 * sin(TAU * PHASE.y), 0.25 * CELL.y + 0.20 * sin(TAU * 2.0 * PHASE.y), ROWS * CELL.z * PHASE.x);
  vec3 direction = normalize(vec3(p, 1.55));
  direction.xy = Rotate(direction.xy, 0.10 * sin(TAU * PHASE.y));
  direction.xz = Rotate(direction.xz, 0.28 * sin(TAU * PHASE.y + 1.0));
  direction.yz = Rotate(direction.yz, 0.10 * sin(TAU * 2.0 * PHASE.y));

  float t = 0.0;
  float glow = 0.0;
  float blob = 1.0;
  vec3 id = vec3(0.0);
  for (int i = 0; i < MAX_STEPS; ++i)
  {
    if (float(i) >= STEPS)
    {
      break;
    }
    float d = max(Field(origin + direction * t, blob, id), 0.0);
    // The light that hangs around the blobs, summed along the ray
    glow += exp(-4.0 * max(blob, 0.0)) * d;
    t += d;
  }

  vec3 sky = Sky(direction);
  vec3 color = sky;
  float reached = 0.004 + 0.0018 * t;
  if (blob < 4.0 * reached)
  {
    // The surface is round, so its normal is measured: the field is asked four more times around the place the ray reached
    vec3 place = origin + direction * t;
    vec2 e = vec2(1.0, -1.0) * 0.012;
    float unused;
    vec3 unusedId;
    float d1;
    float d2;
    float d3;
    float d4;
    unused = Field(place + e.xyy, d1, unusedId);
    unused = Field(place + e.yyx, d2, unusedId);
    unused = Field(place + e.yxy, d3, unusedId);
    unused = Field(place + e.xxx, d4, unusedId);
    vec3 n = normalize(e.xyy * d1 + e.yyx * d2 + e.yxy * d3 + e.xxx * d4);

    // Every cell has one of a few colours: most are blue, some light blue or purple, a few amber
    float h = Hash21(vec2(id.x + 17.0 * id.y, mod(id.z, ROWS)));
    vec3 base = h < 0.52 ? vec3(0.060, 0.330, 1.000)
                         : (h < 0.70 ? vec3(0.100, 0.640, 1.000) : (h < 0.90 ? vec3(0.520, 0.160, 0.980) : vec3(1.000, 0.560, 0.080)));
    vec3 lightDirection = normalize(vec3(-0.45, 0.75, -0.45));
    // Light that wraps around a little, as through something soft
    float diffuse = clamp(0.35 + 0.65 * dot(n, lightDirection), 0.0, 1.0);
    float specular = pow(max(dot(reflect(direction, n), lightDirection), 0.0), 60.0);
    float rim = pow(1.0 - max(dot(n, -direction), 0.0), 2.6);
    float pulse = 0.5 + 0.5 * sin(TAU * (2.0 * PHASE.z + h));
    vec3 lit = base * (0.16 + 0.95 * diffuse) * (0.85 + 0.25 * n.y);
    lit += base * base * (0.30 + 0.30 * pulse);
    lit += vec3(1.0) * 1.05 * specular;
    lit += mix(base, vec3(0.75, 0.90, 1.00), 0.55) * 0.85 * rim;
    // Further blobs fade into the sky, and more steps see further. A blob the ray did not quite reach fades in, so it does not tear.
    float reach = clamp(1.4 * STEPS, 8.0, 220.0);
    float solid = 1.0 - smoothstep(reached, 4.0 * reached, blob);
    color = mix(sky, lit, solid * exp(-2.2 * t / reach));
  }
  color += vec3(0.220, 0.420, 1.000) * 0.16 * glow;

  // A soft vignette keeps the UI on top readable
  float vignette = 1.0 - 0.28 * dot(p * vec2(0.55, 0.8), p * vec2(0.55, 0.8));
  o_color = vec4(color * clamp(vignette, 0.0, 1.0), 1.0);
}
