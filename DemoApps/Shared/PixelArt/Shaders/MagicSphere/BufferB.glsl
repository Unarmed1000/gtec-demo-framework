// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// MAGIC SPHERE - BUFFER B: glow, step 1.
// Keeps the bright parts of the scene (the lights) and blurs them horizontally.
//   iChannel0 = Buffer A
//
// PixelArt: TAPS/SIGMA are named HALO_TAPS/HALO_SIGMA (Buffer C uses the same ones) and the stride is adjustable.

#ifndef HALO_TAPS
#define HALO_TAPS 14
#endif
#ifndef HALO_SIGMA
#define HALO_SIGMA 5.0
#endif
#ifndef HALO_STRIDE
#define HALO_STRIDE 1.0
#endif
#ifndef THRESHOLD
#define THRESHOLD 1.0        // brightness above which things glow
#endif

vec3 bright(vec2 uv) {
    // only the part of each pixel that is brighter than white feeds the glow
    vec3 c = min(texture(iChannel0, uv).rgb, vec3(6.0));
    float l = max(c.r, max(c.g, c.b));
    return c * max(l - THRESHOLD, 0.0) / max(l, 1e-4);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float stride = HALO_STRIDE * iResolution.y / 450.0;   // glow size scales with resolution
    vec3 s = vec3(0.0);
    float wsum = 0.0;
    for (int i = -HALO_TAPS; i <= HALO_TAPS; i++) {
        float x = float(i);
        float w = exp(-x * x / (2.0 * HALO_SIGMA * HALO_SIGMA));
        s += bright((fragCoord + vec2(x * stride, 0.0)) / iResolution.xy) * w;
        wsum += w;
    }
    fragColor = vec4(s / wsum, 1.0);
}
