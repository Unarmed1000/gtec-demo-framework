// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// MAGIC SPHERE - IMAGE: adds two scales of glow to the scene, then tonemaps.
//   iChannel0 = Buffer A (scene)
//   iChannel1 = Buffer C (tight halo)
//   iChannel2 = Buffer D (wide aura, half finished)
//
// PixelArt: TAPS/SIGMA/STRIDE are named AURA_TAPS/AURA_SIGMA/AURA_STRIDE (Buffer D uses the same ones). The app renders to a sRGB
// framebuffer, so this tab writes linear colors and leaves the gamma to the GPU (Scene.json says "Output": "Linear").

#ifndef HALO
#define HALO 0.9             // strength of the tight glow around each light
#endif
#ifndef AURA
#define AURA 1.5             // strength of the broad glow
#endif
#ifndef EXPOSURE
#define EXPOSURE 1.0         // overall brightness before tonemapping
#endif
#ifndef VIGNETTE
#define VIGNETTE 0.3         // how much darker the corners are
#endif

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
    vec2 uv = fragCoord / iResolution.xy;

    // finish the wide blur vertically
    float stride = AURA_STRIDE * iResolution.y / 450.0;
    vec3 aura = vec3(0.0);
    float wsum = 0.0;
    for (int i = -AURA_TAPS; i <= AURA_TAPS; i++) {
        float y = float(i);
        float w = exp(-y * y / (2.0 * AURA_SIGMA * AURA_SIGMA));
        aura += texture(iChannel2, (fragCoord + vec2(0.0, y * stride)) / iResolution.xy).rgb * w;
        wsum += w;
    }
    aura /= wsum;

    vec3 col = texture(iChannel0, uv).rgb
             + texture(iChannel1, uv).rgb * HALO
             + aura * AURA;
    col *= EXPOSURE;

    // tonemap (ACES fit), gamma, vignette
    col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);
    vec2 q = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    float vignette = 1.0 - VIGNETTE * dot(q, q);
#ifdef PIXELART
    // linear output: the vignette is applied before the GPU's gamma, so it is raised to 2.2 to darken the corners just as much
    col = clamp(col, 0.0, 1.0) * pow(max(vignette, 0.0), 2.2);
#else
    col = pow(clamp(col, 0.0, 1.0), vec3(0.4545));
    col *= vignette;
#endif

    fragColor = vec4(col, 1.0);
}
