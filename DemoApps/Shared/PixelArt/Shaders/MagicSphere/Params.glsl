// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The adjustable constants of Magic Sphere. The number in PARAM(n) is the index of the constant in the "Params" list of Scene.json, the
// app checks that both agree when it loads the scene. The tabs only define a constant that is not defined here, so they still run on
// shadertoy as they are.

#define BAND_STEPS int(PARAM(0) + 0.5)
#define VOLUME_STEPS int(PARAM(1) + 0.5)
#define OUTSIDE_STEPS int(PARAM(2) + 0.5)
#define RUNES floor(PARAM(3) + 0.5)
#define BAND_R PARAM(4)
#define BAND_H PARAM(5)
#define FLOOR_Y PARAM(6)
#define IOR PARAM(7)
#define DISP PARAM(8)
#define MILK PARAM(9)
#define ENV_GAIN PARAM(10)
#define PLASMA PARAM(11)
#define SMOKE PARAM(12)
#define GLYPH_GLOW PARAM(13)
#define THRESHOLD PARAM(14)
#define HALO_TAPS int(PARAM(15) + 0.5)
#define HALO_SIGMA PARAM(16)
#define HALO_STRIDE PARAM(17)
#define AURA_TAPS int(PARAM(18) + 0.5)
#define AURA_SIGMA PARAM(19)
#define AURA_STRIDE PARAM(20)
#define HALO PARAM(21)
#define AURA PARAM(22)
#define EXPOSURE PARAM(23)
#define VIGNETTE PARAM(24)
