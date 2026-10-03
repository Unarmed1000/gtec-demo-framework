// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// MAGIC SPHERE - BUFFER D: glow, step 3.
// Blurs the tight halo again, much wider, horizontally. The Image tab finishes
// this blur vertically to get the broad aura of light around the orb.
//   iChannel0 = Buffer C
//
// PixelArt: TAPS/SIGMA/STRIDE are named AURA_TAPS/AURA_SIGMA/AURA_STRIDE (the Image tab uses the same ones).

#ifndef AURA_TAPS
#define AURA_TAPS 22
#endif
#ifndef AURA_SIGMA
#define AURA_SIGMA 9.0
#endif
#ifndef AURA_STRIDE
#define AURA_STRIDE 3.5
#endif

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float stride = AURA_STRIDE * iResolution.y / 450.0;
    vec3 s = vec3(0.0);
    float wsum = 0.0;
    for (int i = -AURA_TAPS; i <= AURA_TAPS; i++) {
        float x = float(i);
        float w = exp(-x * x / (2.0 * AURA_SIGMA * AURA_SIGMA));
        s += texture(iChannel0, (fragCoord + vec2(x * stride, 0.0)) / iResolution.xy).rgb * w;
        wsum += w;
    }
    fragColor = vec4(s / wsum, 1.0);
}
