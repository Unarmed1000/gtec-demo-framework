# Frame pacing measurements

The framework can draw the [mb-framepacing](https://github.com/Unarmed1000/mb-framepacing) frame marker on top of every frame. The
marker is a QR code that contains the app's frame index and the animation time the frame was rendered for. A capture of the display
output (capture card, lossless screen recording or a high speed camera) is then analysed by the mb-framepacing tools, which report
the animation error: the gap between the animation timer and when each frame actually appeared on screen.

The marker is drawn by the host as the very last thing of the frame (after the app, its UI and the stats overlay), so every app
supports it without code changes. It works with OpenGL ES and Vulkan apps.

## Supported platforms

Windows, Ubuntu, macOS, Android, QNX, Emscripten and RDK Yocto. The marker library (`ThirdParty/Recipe/mb_framemarker_0_1`) is
built automatically the first time an app is built. It requires CMake 4.0+, except on Android where the recipe applies
`android-cmake-minimum-version.patch` which lowers the minimum to CMake 3.23 (the tests are disabled, so the library only uses CMake 3.23
features). On other platforms the feature is compiled out and `IFramePacingService` is not available.

The marker needs the basic render system, so it is drawn by OpenGL ES 2, OpenGL ES 3 and Vulkan apps. Hosts without it (OpenVG, G2D,
console and window apps) log a warning and do not draw the marker.

## Command line arguments

Argument                          | Description
----------------------------------|------------------------------------------------------------------------------------------------------
`--FramePacing`                   | Draw the frame marker on top of every frame.
`--FramePacing.ModuleSize <px>`   | The size of one QR module in pixels (default 6).
`--FramePacing.CaptureHeight <px>`| The height the capture is stored at. The module size is calculated so the marker survives the downscale (overrides the module size).
`--FramePacing.Slot <slot>`       | `top` (default), `middle`, `bottom` or `all`. `all` draws the frame marker three times, which detects tearing.
`--FramePacing.Run <name>`        | Start a measured run with this name (at most 64 bytes) at the first frame. Implies `--FramePacing`.
`--FramePacing.Duration <sec>`    | The duration of the measured part of the run (0 = until the app exits).
`--FramePacing.RunId <id>`        | The id of the run (defaults to a random id).

A run is bracketed by a start marker (shown for at least 100ms, it carries the run name and the wall clock start time) and an end
marker, so the analysis can cut the capture to exactly the measured window.

Example, measure 30 seconds of an app while capturing at 960x540:

```bash
GLES3.Stats --FramePacing.Run "Stats 30s" --FramePacing.Duration 30 --FramePacing.CaptureHeight 540
```

```bash
mb-framepacing capture -d "<capture card>" --scale 960x540 --wait-for-start --stop-at-end --analyze
```

## Controlling it from an app

Add a dependency on `FslDemoService.FramePacing` and use the service:

```cpp
#include <FslDemoService/FramePacing/IFramePacingService.hpp>

// The service is null on platforms that do not support the marker
auto framePacing = config.DemoServiceProvider.TryGet<IFramePacingService>();
if (framePacing)
{
  framePacing->SetEnabled(true);
  framePacing->BeginRun("camera pan", TimeSpan::FromSeconds(10));    // zero duration = until EndRun
}
```

A run is either open ended (zero duration, it lasts until `EndRun`) or timed (it ends by itself once the measured part has lasted
the given duration). `GetRunDuration()` and `GetRunMeasuredTime()` report the progress of a timed run.

`IFramePacingService` also exposes the slot, module size, capture height, run state and run id. See the
[GLES2.FramePacing](../DemoApps/GLES2/FramePacing), [GLES3.FramePacing](../DemoApps/GLES3/FramePacing) and
[Vulkan.FramePacing](../DemoApps/Vulkan/FramePacing) samples (they share their code in [Shared/FramePacing](../DemoApps/Shared/FramePacing)).

## What the marker reports

- **Frame index**: a 64 bit counter that increments for every frame the app draws.
- **Animation time**: `FrameInfo::Time.CurrentTickCount`, the time the app's animation was evaluated for (100ns ticks). Fixed time
  step, pause and forced update time modes are reported exactly as the app sees them.

## Notes

- The marker must reach the capture unmodified: it is drawn opaque, pure black/white and pixel aligned at the swapchain resolution.
  Use a lossless capture and keep at least 3 stored pixels per module (`--FramePacing.CaptureHeight` computes that for you).
- HDR swapchains may alter pure black and white, prefer SDR apps for measurements.
- On OpenGL ES the `--Stats` overlay is drawn after the marker, avoid combining the two if the stats overlap the marker slots.
- If the marker is enabled at runtime (instead of on the command line) it is first shown the frame after it was enabled, as the render
  resources it needs are created on demand.
- The marker library is pinned to a commit of mb-framepacing until the first `marker-v0.1.0` release is published.

## Implementation

Package                             | Content
------------------------------------|---------------------------------------------------------------------------------------------------
`ThirdParty/mb_framemarker`         | The mb-framepacing C++ marker library (via `Recipe.mb_framemarker_0_1`).
`FslDemoService.FramePacing`        | The public `IFramePacingService` interface (header only, available on all platforms).
`FslDemoService.FramePacing.Impl`   | The service, its command line options, the run state machine and the overlay that draws the marker.

The overlay renders the triangles produced by `MB::FrameMarker::GenerateTriangles` on the GPU through the FslGraphics3D
`IBasicRenderSystem` (one dynamic vertex buffer, an opaque material without depth test or culling and a pixel aligned orthographic
projection), so the same code is used for OpenGL ES 2, OpenGL ES 3 and Vulkan. The host draws it inside the frame after the app has drawn
(`DemoAppManager` for OpenGL ES and `DemoAppVulkanBasic::AddSystemUI` for Vulkan).
