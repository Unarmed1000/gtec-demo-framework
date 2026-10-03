// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// MAGIC SPHERE - BUFFER C: glow, step 2.
// Blurs Buffer B vertically. The result is the tight halo hugging each light.
//   iChannel0 = Buffer B
//
// PixelArt: TAPS/SIGMA are named HALO_TAPS/HALO_SIGMA (Buffer B uses the same ones) and the stride is adjustable.

#ifndef HALO_TAPS
#define HALO_TAPS 14
#endif
#ifndef HALO_SIGMA
#define HALO_SIGMA 5.0
#endif
#ifndef HALO_STRIDE
#define HALO_STRIDE 1.0
#endif

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    float stride = HALO_STRIDE * iResolution.y / 450.0;
    vec3 s = vec3(0.0);
    float wsum = 0.0;
    for (int i = -HALO_TAPS; i <= HALO_TAPS; i++) {
        float y = float(i);
        float w = exp(-y * y / (2.0 * HALO_SIGMA * HALO_SIGMA));
        s += texture(iChannel0, (fragCoord + vec2(0.0, y * stride)) / iResolution.xy).rgb * w;
        wsum += w;
    }
    fragColor = vec4(s / wsum, 1.0);
}
