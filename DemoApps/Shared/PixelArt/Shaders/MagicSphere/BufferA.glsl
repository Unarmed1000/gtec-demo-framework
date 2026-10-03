// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// MAGIC SPHERE - BUFFER A: the scene, rendered in HDR (no tonemapping here).
// A solid glass orb full of golden plasma and stars, wrapped in a gold band
// with engraved, light-filled glyphs. Drag the mouse to orbit.
//
// Shadertoy setup (5 tabs, add buffers with the "+" next to the Image tab):
//   Buffer A : this file           - iChannel0 = a CUBEMAP (click the channel, "Cubemaps" tab;
//                                    in its settings set Filter to "mipmap" for soft reflections on the gold)
//   Buffer B : BufferB.glsl        - iChannel0 = Buffer A
//   Buffer C : BufferC.glsl        - iChannel0 = Buffer B
//   Buffer D : BufferD.glsl        - iChannel0 = Buffer C
//   Image    : Image.glsl          - iChannel0 = Buffer A, iChannel1 = Buffer C, iChannel2 = Buffer D
//
// PixelArt: the same setup is in Scene.json, and the constants below are adjustable in the app (see Params.glsl).

#define PI  3.14159265
#define TAU 6.28318531

#ifndef BAND_STEPS
#define BAND_STEPS 110
#endif
#ifndef VOLUME_STEPS
#define VOLUME_STEPS 64       // lower to ~36 on slow GPUs
#endif
#ifndef OUTSIDE_STEPS
#define OUTSIDE_STEPS 48      // steps of a ray that left the glass
#endif
#ifndef RUNES
#define RUNES 18.0            // glyphs around the band
#endif
#ifndef BAND_R
#define BAND_R 1.035          // band radius (mid-thickness)
#endif
#ifndef BAND_H
#define BAND_H 0.17           // band half-width
#endif
#ifndef FLOOR_Y
#define FLOOR_Y -1.11
#endif
#ifndef IOR
#define IOR  1.46             // refractive index of the glass
#endif
#ifndef DISP
#define DISP 0.010            // dispersion: IOR spread between red and blue
#endif
#ifndef MILK
#define MILK 0.008            // faint whiteness of the glass body
#endif
#ifndef ENV_GAIN
#define ENV_GAIN 0.4          // brightness of the cubemap reflections
#endif
#ifndef PLASMA
#define PLASMA 3.5            // brightness of the plasma inside the orb
#endif
#ifndef SMOKE
#define SMOKE 0.0             // how thick the smoke is (0 = pure light, no smoke; try 4.0 for smoke)
#endif
#ifndef GLYPH_GLOW
#define GLYPH_GLOW 6.0        // brightness of the light in the engraved glyphs
#endif

mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }

// ---------------------------------------------------------------- noise
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
    vec3 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i + vec3(0, 0, 0)), hash(i + vec3(1, 0, 0)), f.x),
                   mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
                   mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm3(vec3 p) {
    return (noise(p) + 0.5 * noise(p * 2.03 + 7.1) + 0.25 * noise(p * 4.1 + 13.7)) / 1.75;
}

// ---------------------------------------------------------------- lighting
const vec3 L_KEY  = vec3(-0.5145, 0.7717, 0.3730);   // normalised light directions
const vec3 L_FILL = vec3(0.7683, 0.3293, -0.5488);
const vec3 L_RIM  = vec3(0.1990, 0.3980, -0.8955);

// Environment from the cubemap in iChannel0. Cubemaps are 8-bit, so the
// brightest texels are pushed up to stand in for real light sources.
vec3 cube(vec3 d, float lod) {
#ifdef PIXELART
    // PixelArt reads the cubemap as sRGB, so the GPU already turned it into linear colors
    vec3 c = textureLod(iChannel0, d, lod).rgb;
#else
    vec3 c = pow(textureLod(iChannel0, d, lod).rgb, vec3(2.2));
#endif
    float m = max(c.r, max(c.g, c.b));
    return c * (1.0 + 1.5 * m * m * m * m) * ENV_GAIN;
}

vec3 env(vec3 d) { return cube(d, 0.0); }                               // sharp, for glass
vec3 envMetal(vec3 d, float rough) { return cube(d, rough * 8.0); }     // blurred by roughness

// ---------------------------------------------------------------- PBR gold
// Cook-Torrance specular: GGX distribution, Smith-Schlick geometry, Schlick fresnel.
vec3 ggx(vec3 n, vec3 v, vec3 l, float rough, vec3 F0) {
    vec3 h = normalize(v + l);
    float nl = max(dot(n, l), 0.0);
    float nv = max(dot(n, v), 1e-3);
    float nh = max(dot(n, h), 0.0);
    float vh = max(dot(v, h), 0.0);
    float a = rough * rough, a2 = a * a;
    float dd = nh * nh * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * dd * dd);
    float k = (rough + 1.0); k = k * k / 8.0;
    float G = nl / (nl * (1.0 - k) + k) * nv / (nv * (1.0 - k) + k);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - vh, 5.0);
    return D * G * F / (4.0 * nl * nv + 1e-3) * nl;
}

// Pure metal: no diffuse, coloured reflections. 'extra' is additional light
// arriving at the surface (glow from the engraved glyphs).
vec3 pbrGold(vec3 p, vec3 n, vec3 v, float rough, vec3 extra) {
    vec3 F0 = vec3(1.0, 0.766, 0.336);
    vec3 col = vec3(0.0);

    // the glowing interior of the orb, lighting faces turned toward it.
    // It is a large, soft light, so its highlight is wide (high roughness) and capped.
    vec3 lo = -normalize(p);
    vec3 orbLight = vec3(1.0, 0.70, 0.30);
    col += min(ggx(n, v, lo, max(rough, 0.6), F0), vec3(2.0)) * orbLight * 1.2;
    col += F0 * orbLight * max(dot(n, lo), 0.0) * 0.2;

    // environment reflection
    float fres = pow(clamp(1.0 - dot(n, v), 0.0, 1.0), 5.0);
    col += (F0 + (1.0 - F0) * fres) * envMetal(reflect(-v, n), rough);

    return col + F0 * extra;
}

// ---------------------------------------------------------------- glyphs
float sdSeg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    return length(pa - ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0));
}

float ring(vec2 p, vec2 c, float r) { return abs(length(p - c) - r); }

// Distance to one of eight hand-drawn symbols, in a unit cell (x right, y up).
float runeDist(vec2 p, float id) {
    float k = mod(id * 3.0 + 1.0, 8.0);
    float d;
    if (k < 0.5) {            // triangle
        d = min(min(sdSeg(p, vec2(0.10, 0.10), vec2(0.90, 0.10)),
                    sdSeg(p, vec2(0.90, 0.10), vec2(0.50, 0.90))),
                    sdSeg(p, vec2(0.50, 0.90), vec2(0.10, 0.10)));
    } else if (k < 1.5) {     // barred peak
        d = min(min(sdSeg(p, vec2(0.12, 0.00), vec2(0.50, 1.00)),
                    sdSeg(p, vec2(0.50, 1.00), vec2(0.88, 0.00))),
                    sdSeg(p, vec2(0.27, 0.40), vec2(0.73, 0.40)));
    } else if (k < 2.5) {     // arrow
        d = min(sdSeg(p, vec2(0.50, 0.00), vec2(0.50, 1.00)),
            min(sdSeg(p, vec2(0.50, 1.00), vec2(0.15, 0.62)),
                sdSeg(p, vec2(0.50, 1.00), vec2(0.85, 0.62))));
        d = min(d, sdSeg(p, vec2(0.25, 0.22), vec2(0.75, 0.22)));
    } else if (k < 3.5) {     // circle on a stave
        d = min(ring(p, vec2(0.50, 0.50), 0.30), sdSeg(p, vec2(0.50, 0.00), vec2(0.50, 1.00)));
    } else if (k < 4.5) {     // double pennant
        d = sdSeg(p, vec2(0.22, 0.00), vec2(0.22, 1.00));
        d = min(d, min(sdSeg(p, vec2(0.22, 1.00), vec2(0.80, 0.75)),
                       sdSeg(p, vec2(0.80, 0.75), vec2(0.22, 0.50))));
        d = min(d, min(sdSeg(p, vec2(0.22, 0.50), vec2(0.80, 0.25)),
                       sdSeg(p, vec2(0.80, 0.25), vec2(0.22, 0.00))));
    } else if (k < 5.5) {     // diamond with a tail
        d = min(min(sdSeg(p, vec2(0.50, 1.00), vec2(0.85, 0.65)),
                    sdSeg(p, vec2(0.85, 0.65), vec2(0.50, 0.30))),
                min(sdSeg(p, vec2(0.50, 0.30), vec2(0.15, 0.65)),
                    sdSeg(p, vec2(0.15, 0.65), vec2(0.50, 1.00))));
        d = min(d, sdSeg(p, vec2(0.50, 0.30), vec2(0.50, 0.00)));
    } else if (k < 6.5) {     // cross under a bar
        d = min(sdSeg(p, vec2(0.15, 0.00), vec2(0.85, 0.85)),
                sdSeg(p, vec2(0.85, 0.00), vec2(0.15, 0.85)));
        d = min(d, sdSeg(p, vec2(0.20, 1.00), vec2(0.80, 1.00)));
    } else {                  // orb on a cross
        d = min(ring(p, vec2(0.50, 0.68), 0.27), sdSeg(p, vec2(0.50, 0.41), vec2(0.50, 0.00)));
        d = min(d, sdSeg(p, vec2(0.28, 0.18), vec2(0.72, 0.18)));
    }
    return d;
}

// ---------------------------------------------------------------- band geometry
// The band's own frame: tilted, and slowly sliding around the orb.
vec3 bandSpace(vec3 p) {
    p.xy *= rot(0.85);
    p.yz *= rot(0.25);
    p.xz *= rot(iTime * 0.15);
    return p;
}

float bandWave(float ang) { return 0.13 * sin(2.0 * ang); }   // gives the band its S-curve

// Glyph cell coordinates at a point on the band; y is the offset across the band.
vec2 runeCell(float ang, float y, out float cid) {
    float u = (ang / TAU + 0.5) * RUNES;
    cid = floor(u);
    return vec2((fract(u) - 0.25) / 0.5, (y / BAND_H + 0.6) / 1.2);
}

// The band: a strip wrapped round the orb with a rounded lip on each edge.
// The glyphs and two border lines are really carved into its outer face.
float sdBand(vec3 p) {
    vec3 q = bandSpace(p);
    float r = length(q);
    float ang = atan(q.z, q.x);
    float y = q.y - bandWave(ang);
    float core = max(abs(r - BAND_R) - 0.016, abs(y) - BAND_H);
    if (core < 0.02) {
        float cid;
        vec2 c = runeCell(ang, y, cid);
        float glyph = max((runeDist(c, cid) - 0.055) * 0.17, (BAND_R + 0.004) - r);
        float line = max(abs(abs(y) - 0.128) - 0.004, (BAND_R + 0.009) - r);
        core = max(core, -min(glyph, line));
    }
    float lip = length(vec2(r - BAND_R - 0.004, abs(y) - BAND_H)) - 0.028;
    return min(core, lip);
}

float mapBand(vec3 p, out float id) {
    id = 0.0;
    return sdBand(p);
}

vec3 calcNormal(vec3 p) {
    vec2 e = vec2(1.0, -1.0) * 0.0012;
    float i;
    return normalize(e.xyy * mapBand(p + e.xyy, i) + e.yyx * mapBand(p + e.yyx, i) +
                     e.yxy * mapBand(p + e.yxy, i) + e.xxx * mapBand(p + e.xxx, i));
}

vec3 shadeBand(vec3 p, vec3 n, vec3 rd, float id) {
    vec3 v = -rd;
    vec3 q = bandSpace(p);
    float r = length(q);
    float ang = atan(q.z, q.x);
    float y = q.y - bandWave(ang);
    float cid;
    vec2 c = runeCell(ang, y, cid);
    float runeD = runeDist(c, cid);

    vec3 runeCol = vec3(1.0, 0.60, 0.16);
    float flick = 0.85 + 0.15 * sin(iTime * 2.0 + cid * 1.9);
    float outer = step(BAND_R, r);                       // outer face of the band

    // light from the glyph grooves washing over the gold around them
    float spill = exp(-max(runeD - 0.055, 0.0) * 9.0) * outer * step(abs(y), BAND_H * 0.8);
    float rough = 0.26 + 0.08 * noise(q * 40.0);
    vec3 col = pbrGold(p, n, v, rough, runeCol * spill * flick * 1.5);

    // the floor of each engraved glyph is filled with light (HDR, so it blooms)
    float lit = smoothstep(0.06, 0.035, runeD) * step(r, BAND_R + 0.0075) * outer;
    return mix(col, (runeCol + vec3(1.0, 0.9, 0.6) * 0.3) * GLYPH_GLOW * flick, lit);
}

// ---------------------------------------------------------------- inside the orb
// Golden plasma smoke: thin, folded sheets of light with a soft body of smoke
// around each one. The sheets are the level surfaces of a warped noise field,
// so they curl and nest inside each other; the field rises and twists, fed by
// a hot source at the bottom. Returns emitted light (rgb) and smoke density (a).
vec4 plasma(vec3 p, float pulse) {
    float r = length(p);
    vec3 q = p;
    q.xz *= rot(0.6 * q.y + iTime * 0.15);               // slow twist about the vertical
    q.y -= iTime * 0.18;                                 // rising
    q += (vec3(noise(q * 1.3 + 3.0), noise(q * 1.3 + 17.0), noise(q * 1.3 + 31.0)) - 0.5) * 0.9;

    float f = noise(q * 1.6) + 0.5 * noise(q * 3.3 + 5.0);
    float ridge = 1.0 - abs(sin(f * PI * 3.0));
    float sheet = pow(ridge, 7.0);                                   // bright core of each sheet
    float body = pow(ridge, 2.5);                                    // the smoke around it
    float mask = 0.25 + smoothstep(0.35, 0.7, noise(q * 0.9 + 40.0));   // broken into bands
    float low = smoothstep(1.0, -1.0, p.y);                          // strongest near the bottom
    float heat = 0.5 + 1.8 * low * low;
    float edge = smoothstep(1.0, 0.9, r);

    vec3 c = mix(vec3(1.0, 0.45, 0.05), vec3(1.0, 0.85, 0.40), sheet);
    vec3 e = c * sheet * mask * heat * edge * PLASMA;
    e += vec3(1.0, 0.40, 0.06) * body * mask * heat * edge * PLASMA * 0.12 * min(SMOKE, 1.0);   // smoke lit from within

    vec3 s = p - vec3(0.0, -0.95, 0.0);                              // the source
    e += vec3(1.0, 0.80, 0.40) * exp(-dot(s, s) * 14.0) * 1.2 * PLASMA;

    return vec4(e * pulse, body * mask * edge * SMOKE);
}

// Hair-thin golden threads: the curves where two noise fields cross their mid level.
vec3 wisps(vec3 p) {
    float r = length(p);
    vec3 q = p;
    q.xz *= rot(iTime * 0.2 + q.y * 1.2);
    q.xy *= rot(iTime * 0.07);
    float a = noise(q * 2.3) + 0.5 * noise(q * 4.9 + 3.1) - 0.75;
    float b = noise(q.zxy * 2.1 + 17.0) + 0.5 * noise(q.yzx * 4.3 + 5.0) - 0.75;
    float line = 1.0 / (1.0 + (a * a + b * b) * 700.0);
    float w = smoothstep(0.35, 0.9, r) * smoothstep(1.0, 0.94, r)
            * (0.35 + 0.65 * smoothstep(0.5, -0.7, p.y)) + 0.15;
    return vec3(1.0, 0.68, 0.25) * line * w;
}

// Stars at a point on a shell inside the ball: many faint, a few bright, twinkling.
float starLayer(vec3 p, float scale, float seed) {
    p.xz *= rot(iTime * 0.03 + seed);
    vec3 g = p * scale + seed * 7.3;
    vec3 i = floor(g);
    float h = hash(i);
    if (h < 0.55) return 0.0;
    vec3 o = (vec3(hash(i + 1.3), hash(i + 2.7), hash(i + 4.1)) - 0.5) * 0.5;
    float d = length(fract(g) - 0.5 - o);
    float mag = pow((h - 0.55) / 0.45, 3.0);
    float tw = 0.75 + 0.25 * sin(iTime * (1.5 + h * 4.0) + h * 50.0);
    return (0.15 + mag) * tw * pow(max(1.0 - d * 4.0, 0.0), 6.0);
}

// ---------------------------------------------------------------- backdrop
vec3 backdrop(vec3 ro, vec3 rd, vec2 uv, float pulse) {
    vec3 c = vec3(0.085, 0.085, 0.09) * (1.0 - 0.5 * length(uv));
    if (rd.y < 0.0) {
        float tg = (FLOOR_Y - ro.y) / rd.y;
        vec3 g = ro + rd * tg;
        float dd = length(g.xz);

        // the orb's shadow from the key light, with the caustic a glass ball
        // focuses into the middle of it (split into colours by dispersion)
        float sb = dot(g, L_KEY);
        float ds = length(g - L_KEY * sb);               // how close the light ray passes to the orb's centre
        float shadow = sb < 0.0 ? smoothstep(0.75, 1.1, ds) : 1.0;
        vec3 cs = ds * vec3(0.90, 1.0, 1.10);
        vec3 caustic = sb < 0.0 ? exp(-cs * cs * 14.0) : vec3(0.0);

        vec3 gc = vec3(0.03, 0.03, 0.034) + vec3(0.03) * shadow;
        gc += vec3(1.0, 0.95, 0.85) * caustic * 0.45;
        gc += vec3(1.0, 0.6, 0.2) * 0.55 * exp(-dd * dd * 0.45) * pulse;    // the orb's own light on the floor
        gc *= 1.0 - 0.5 * exp(-dd * dd * 6.0);                             // contact shadow

        // soft reflection of the orb in the glossy floor
        vec3 rr = reflect(rd, vec3(0.0, 1.0, 0.0));
        float rb = dot(g, rr);
        float rh = rb * rb - (dot(g, g) - 1.0);
        if (rh > 0.0 && rb < 0.0) gc += vec3(1.0, 0.7, 0.3) * 0.15 * pulse;

        c = mix(gc, c, 1.0 - exp(-max(tg - 4.0, 0.0) * 0.2));
    }
    return c;
}

// What a ray leaving the glass sees: the band if it runs into it,
// otherwise the room.
vec3 traceOutside(vec3 o, vec3 d, float pulse) {
    float t = 0.0, id = 0.0;
    for (int i = 0; i < OUTSIDE_STEPS; i++) {
        float dist = mapBand(o + d * t, id);
        if (dist < 0.001) {
            vec3 p = o + d * t;
            return shadeBand(p, calcNormal(p), d, id);
        }
        t += dist * 0.7;
        if (t > 0.9) break;
    }
    return backdrop(o, d, vec2(0.0), pulse);
}

// ---------------------------------------------------------------- main
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;

    // camera
    vec2 m = iMouse.z > 0.0
        ? vec2((iMouse.x / iResolution.x - 0.5) * TAU, 0.05 + iMouse.y / iResolution.y * 1.1)
        : vec2(iTime * 0.15, 0.28);
    vec3 ta = vec3(0.0, -0.05, 0.0);
    vec3 ro = ta + 4.4 * vec3(cos(m.y) * sin(m.x), sin(m.y), cos(m.y) * cos(m.x));
    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(ww, vec3(0.0, 1.0, 0.0)));
    vec3 vv = cross(uu, ww);
    vec3 rd = normalize(uv.x * uu + uv.y * vv + 1.8 * ww);

    float pulse = 0.85 + 0.15 * sin(iTime * 1.7);
    vec3 col = backdrop(ro, rd, uv, pulse);

    float b = dot(ro, rd);
    float c0 = dot(ro, ro);

    // --- the glass sphere (analytic)
    float hS = b * b - (c0 - 1.0);
    bool hitS = hS > 0.0;
    float t0 = 1e5;
    if (hitS) t0 = -b - sqrt(hS);

    // --- march the band, as far as the front of the glass
    bool hitB = false;
    float tB = 0.0, idB = 0.0;
    float hB = b * b - (c0 - 1.3 * 1.3);
    if (hB > 0.0) {
        hB = sqrt(hB);
        tB = max(-b - hB, 0.0);
        float tEnd = min(-b + hB, t0);
        for (int i = 0; i < BAND_STEPS; i++) {
            float d = mapBand(ro + rd * tB, idB);
            if (d < 0.0008) { hitB = true; break; }
            tB += d * 0.7;
            if (tB > tEnd) break;
        }
    }

    if (hitB) {
        // band in front of the glass
        vec3 p = ro + rd * tB;
        col = shadeBand(p, calcNormal(p), rd, idB);
    } else if (hitS) {
        // --- solid glass ball
        vec3 pin = ro + rd * t0;
        vec3 n = pin;
        float ndv = max(dot(n, -rd), 0.0);

        // refract in and out once per colour: the slightly different bend for
        // red, green and blue gives coloured fringes on whatever lies behind
        vec3 behind = vec3(0.0);
        vec3 r1g = rd, peg = pin;
        float chord = 0.0;
        for (int k = 0; k < 3; k++) {
            float ior = IOR + DISP * (float(k) - 1.0);
            vec3 r1 = refract(rd, n, 1.0 / ior);
            float ch = -2.0 * dot(n, r1);
            vec3 pe = pin + r1 * ch;
            vec3 r2 = refract(r1, -pe, ior);
            if (dot(r2, r2) < 0.5) r2 = reflect(r1, -pe);
            behind[k] = traceOutside(pe, r2, pulse)[k];
            if (k == 1) { r1g = r1; peg = pe; chord = ch; }
        }

        // the magic inside, seen along the bent ray (so the ball magnifies it)
        vec3 vol = vec3(0.0);
        float trans = 1.0;                               // how much light still gets through the smoke
        float dt = chord / float(VOLUME_STEPS);
        float jitter = fract(sin(dot(fragCoord, vec2(12.9898, 78.233))) * 43758.5453);
        float tv = dt * jitter;
        for (int i = 0; i < VOLUME_STEPS; i++) {
            vec3 p = pin + r1g * tv;
            vec4 pl = plasma(p, pulse);
            vol += trans * (pl.rgb + wisps(p) * 3.0) * dt;
            trans *= exp(-pl.a * dt);                    // smoke in front hides what is behind
            tv += dt;
        }

        // a starfield with depth: stars on five nested shells inside the ball
        vec3 stars = vec3(0.0);
        float bm = dot(pin, r1g);
        for (int k = 0; k < 5; k++) {
            float fk = float(k);
            float R = 0.25 + 0.15 * fk;
            float hM = bm * bm - (1.0 - R * R);
            if (hM > 0.0) {
                hM = sqrt(hM);
                vec3 tint = mix(vec3(0.7, 0.85, 1.0), vec3(1.0, 0.85, 0.6), hash(vec3(fk, 9.0, 4.0)));
                stars += tint * (starLayer(pin + r1g * (-bm - hM), 24.0 - 3.0 * fk, fk)
                               + starLayer(pin + r1g * (-bm + hM), 24.0 - 3.0 * fk, fk));
            }
        }

        // glass body: slight tint with thickness, a touch of milkiness
        vec3 absorb = exp(-chord * vec3(0.10, 0.04, 0.05));
        float F = 0.04 + 0.96 * pow(1.0 - ndv, 5.0);                    // fresnel
        col = (behind * absorb * trans + vol + stars * 8.0 * (0.3 + 0.7 * trans)) * (1.0 - F);
        col += vec3(0.9, 0.95, 1.0) * MILK * chord;

        // light bouncing once off the inside of the far wall
        col += env(reflect(r1g, -peg)) * 0.05 * absorb * trans;

        // front surface: reflections of the environment, tinted by a thin film
        vec3 film = 0.5 + 0.5 * cos(TAU * (ndv * 1.6 + noise(n * 2.5 + iTime * 0.05) * 1.2
                                           + vec3(0.0, 0.33, 0.67)));
        col += env(reflect(rd, n)) * (F + 0.03) * mix(vec3(1.0), film * 1.6, 0.35);
        col += film * vec3(0.7, 0.8, 1.2) * pow(1.0 - ndv, 1.5) * 0.18;
    }

    // light escaping the orb into the air around it
    float dc = length(ro + rd * max(-b, 0.0));
    if (!hitB && dc > 1.0) col += vec3(1.0, 0.62, 0.2) * exp(-(dc - 1.0) * 3.0) * 0.1 * pulse;

    fragColor = vec4(max(col, 0.0), 1.0);   // linear HDR; tonemapping happens in the Image tab
}
