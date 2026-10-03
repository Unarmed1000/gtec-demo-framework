// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The adjustable constants of Raymarching Primitives. The number in PARAM(n) is the index of the constant in the "Params" list of
// Scene.json, the app checks that both agree when it loads the scene. Image.glsl only defines a constant that is not defined here, so it
// still runs on shadertoy as it is.

#define AA int(PARAM(0) + 0.5)
#define MARCH_STEPS int(PARAM(1) + 0.5)
