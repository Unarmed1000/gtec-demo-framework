<!-- #AG_DEMOAPP_HEADER_BEGIN# -->
# FramePacing
<img src="Example.jpg" height="135px">

<!-- #AG_DEMOAPP_HEADER_END# -->
<!-- #AG_BRIEF_BEGIN# -->
Shows how to control the [mb-framepacing](https://github.com/Unarmed1000/mb-framepacing) frame marker through the IFramePacingService.
<!-- #AG_BRIEF_END# -->

The host draws a QR frame marker (frame index + animation time) on top of every frame so a capture of the display output can be analysed
for animation error. Every app supports it through the `--FramePacing` command line arguments, this sample just enables it by default
and lets you control measured runs:

- **Space** or **Start run**: start an open ended run, press again to end it.
- **T** or **Start timed run**: start a run that ends by itself after the duration selected with the slider (1-120 seconds).

The moving bar and box are animated from the same animation time the marker reports, so any hitch is visible and measurable.

The sample code lives in [Shared/FramePacing](../../Shared/FramePacing) and only uses the API independent INativeBatch2D, so the
GLES2, GLES3 and Vulkan versions are identical.

See [Doc/FramePacing.md](../../../Doc/FramePacing.md) for details.
