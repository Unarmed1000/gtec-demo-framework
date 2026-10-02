#version 450
// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The background of the FramePacing sample: one triangle that covers the screen, the fragment shader does the work.

void main()
{
  // (-1,-1) (3,-1) (-1,3)
  const vec2 position = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2)) * 2.0 - 1.0;
  gl_Position = vec4(position, 0.0, 1.0);
}
