#version 450
#extension GL_GOOGLE_include_directive : require
// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The Image tab of the MagicSphere scene. The iChannel types must match the channels of the tab in Scene.json (binding 1 + N is
// iChannelN, an unused channel is a sampler2D).

#include "../../../../../Shared/PixelArt/Shaders/Common/PixelArtVulkan.glsl"

layout(set = 0, binding = 1) uniform sampler2D iChannel0;
layout(set = 0, binding = 2) uniform sampler2D iChannel1;
layout(set = 0, binding = 3) uniform sampler2D iChannel2;
layout(set = 0, binding = 4) uniform sampler2D iChannel3;

// Params.glsl defines the adjustable constants before the tab, which only defines the ones that are not defined
#include "../../../../../Shared/PixelArt/Shaders/MagicSphere/Params.glsl"

// The tab
#include "../../../../../Shared/PixelArt/Shaders/MagicSphere/Image.glsl"
