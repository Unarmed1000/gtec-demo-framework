// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The adjustable constants of Let's Self Reflect. The number in PARAM(n) is the index of the constant in the "Params" list of Scene.json,
// the app checks that both agree when it loads the scene. Image.glsl only defines a constant that is not defined here, so it still runs on
// shadertoy as it is.

#define rotation_speed PARAM(0)
#define poly_U PARAM(1)
#define poly_V PARAM(2)
#define poly_W PARAM(3)
#define poly_type int(PARAM(4) + 0.5)
#define poly_zoom PARAM(5)
#define inner_sphere PARAM(6)
#define refr_index PARAM(7)
#define MAX_BOUNCES2 int(PARAM(8) + 0.5)
