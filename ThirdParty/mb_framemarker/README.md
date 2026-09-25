# mb_framemarker

The C++ client library of [mb-framepacing](https://github.com/Unarmed1000/mb-framepacing). It generates a pixel aligned QR frame
marker (frame index + animation time) that is drawn into every frame, so a capture of the display output can be analysed for animation
error (the gap between the animation timer and when each frame actually appears on screen).

The framework integration lives in `FslDemoService.FramePacing` (see [Doc/FramePacing.md](../../Doc/FramePacing.md)).

License: BSD 3-Clause (the library also compiles in qrcodegen, MIT). For more information see the
[official repository](https://github.com/Unarmed1000/mb-framepacing).
