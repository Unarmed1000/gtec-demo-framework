// BSD 3-Clause License
// Copyright (c) 2026, Mana Battery
// All rights reserved.
//
// The background of the FramePacing sample: a quad that covers the screen, the fragment shader does the work.

attribute vec3 VertexPosition;

void main()
{
  gl_Position = vec4(VertexPosition, 1.0);
}
