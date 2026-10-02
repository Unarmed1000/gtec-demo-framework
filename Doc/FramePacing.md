# Frame pacing measurements

The framework can draw the [mb-framepacing](https://github.com/Unarmed1000/mb-framepacing) frame marker on top of every frame. The
marker is a QR code that contains the app's frame index and the animation time the frame was rendered for. A capture of the display
output (capture card, lossless screen recording or a high speed camera) is then analysed by the mb-framepacing tools, which report
the animation error: the gap between the animation timer and when each frame actually appeared on screen.

The marker is drawn by the host as the very last thing of the frame (after the app, its UI and the stats overlay), so every app
supports it without code changes. It works with OpenGL ES and Vulkan apps.

## Supported platforms

Windows, Ubuntu, macOS, Android, QNX, Emscripten and RDK Yocto. The mb-framepacing C++ SDK (`ThirdParty/Recipe/mb_framepacing_0_1`: its
marker module, its experimental pacer module and the core they link) is built automatically the first time an app is built. It requires
CMake 4.0+, except on Android where the recipe applies `android-cmake-minimum-version.patch` which lowers the minimum to CMake 3.30 (the
tests are disabled, so the library only uses CMake 3.30 features). On other platforms the feature is compiled out and
`IFramePacingMarkerService` is not available.

The marker needs the basic render system, so it is drawn by OpenGL ES 2, OpenGL ES 3 and Vulkan apps. Hosts without it (OpenVG, G2D,
console and window apps) log a warning and do not draw the marker.

## Command line arguments

Argument                          | Description
----------------------------------|------------------------------------------------------------------------------------------------------
`--FramePacing`                   | Draw the frame marker on top of every frame.
`--FramePacing.ModuleSize <px>`   | The size of one QR module in pixels (default 6).
`--FramePacing.CaptureHeight <px>`| The height the capture is stored at. The module size is calculated so the marker survives the downscale (overrides the module size).
`--FramePacing.SyncMarker`        | Also draw the small sync marker (the run id and the frame index) at the bottom left. The analysis detects tearing when the two markers disagree, and camera capture needs it for its timing.
`--FramePacing.Run <name>`        | Start a measured run with this name at the first frame. The name is written to the log next to the run's random sequence id. Implies `--FramePacing`.
`--FramePacing.Duration <sec>`    | The duration of the measured part of the run (0 = until the app exits).
`--FramePacing.RunId <id>`        | The id of the run (defaults to a random id).

A run is bracketed by a start marker (shown for at least 100ms, it carries the run's sequence id and the wall clock start time) and an
end marker, so the analysis can cut the capture to exactly the measured window. The sequence id is 16 random bytes, which the
mb-framepacing tools show as 32 hex digits. The run name is not part of the marker, but the log line of the run shows the name, the
run id and the sequence id, so a capture can be matched to the run that produced it.

Example, measure 30 seconds of an app while capturing at 960x540:

```bash
GLES3.Stats --FramePacing.Run "Stats 30s" --FramePacing.Duration 30 --FramePacing.CaptureHeight 540
```

```bash
mb-framepacing capture -d "<capture card>" --scale 960x540 --wait-for-start --stop-at-end --analyze
```

## Controlling it from an app

Add a dependency on `FslDemoService.FramePacingMarker` and use the service:

```cpp
#include <FslDemoService/FramePacingMarker/IFramePacingMarkerService.hpp>

// The service is null on platforms that do not support the marker
auto framePacing = config.DemoServiceProvider.TryGet<IFramePacingMarkerService>();
if (framePacing)
{
  framePacing->SetEnabled(true);
  framePacing->BeginRun("camera pan", TimeSpan::FromSeconds(10));    // zero duration = until EndRun
}
```

A run is either open ended (zero duration, it lasts until `EndRun`) or timed (it ends by itself once the measured part has lasted
the given duration). `GetRunDuration()` and `GetRunMeasuredTime()` report the progress of a timed run.

`IFramePacingMarkerService` also exposes the sync marker, module size, capture height, run state and run id.

`TryGetLastMarker(FramePacingMarkerInfo&)` returns every value the last drawn marker carried: kind, frame index, animation time,
run id, intended display time, target frame time, CPU start time, CPU busy, preferred frame time, the static flag, and for start
markers the run start time and sequence id (`FramePacingSequenceId`). Times use `TimeSpan`/`TickCount`, and a value the framework does
not know (the intended display time, target frame time and preferred frame time, as it has no frame pacer) is an empty `std::optional`. The marker is drawn at the end of the frame, so during a frame
this is the previous frame's marker.

See the [GLES2.FramePacing](../DemoApps/GLES2/FramePacing), [GLES3.FramePacing](../DemoApps/GLES3/FramePacing) and
[Vulkan.FramePacing](../DemoApps/Vulkan/FramePacing) samples (they share their code in [Shared/FramePacing](../DemoApps/Shared/FramePacing)):
they show every value of the last marker in a panel, formatted into a reused `fmt::memory_buffer` so updating it every frame does not
allocate. `--HideMarkerStats` hides the panel.

## What the marker reports

- **Frame index**: a 64 bit counter that increments for every frame the app draws.
- **Animation time**: `FrameInfo::Time.CurrentTickCount`, the time the app's animation was evaluated for (100ns ticks). Fixed time
  step, pause and forced update time modes are reported exactly as the app sees them.
- **Intended display time, target frame time and preferred frame time**: reported as unknown (0) as the framework has no frame pacer, so
  the analysis measures every frame against the display's refresh rate (or the `--target-fps` given to the analysis).
- **Static flag**: never set, the framework does not know when nothing animates, so the analysis judges the animation error of every
  frame.
- **CPU start time and CPU busy**: the CPU start time is taken by the host just before the app update of the frame, and CPU busy is
  the time from then until the marker is drawn (the last thing before the frame is presented). The analysis derives the frametime
  from the step between the CPU start times.
- **Sync marker**: the run id and the frame index, so the analysis can match it to the main marker of the same run.

## Notes

- The marker must reach the capture unmodified: it is drawn opaque, pure black/white and pixel aligned at the swapchain resolution.
  Use a lossless capture and keep at least 3 stored pixels per module (`--FramePacing.CaptureHeight` computes that for you).
- HDR swapchains may alter pure black and white, prefer SDR apps for measurements.
- On OpenGL ES the `--Stats` overlay is drawn after the marker, avoid combining the two if the stats overlap the markers.
- If the marker is enabled at runtime (instead of on the command line) it is first shown the frame after it was enabled, as the render
  resources it needs are created on demand.
- The SDK is pinned to a commit of mb-framepacing until the first `sdk-v0.1.0` release is published.

## Implementation

Package                                    | Content
-------------------------------------------|--------------------------------------------------------------------------------------------
`ThirdParty/mb_framepacing`                | The mb-framepacing C++ SDK, its marker module (via `Recipe.mb_framepacing_0_1`).
`FslDemoService.FramePacingMarker`         | The public `IFramePacingMarkerService` interface (header only, available on all platforms).
`FslDemoService.FramePacingMarker.Control` | The host side `IFramePacingMarkerServiceControl` and `IFramePacingOverlay` interfaces (header only, available on all platforms).
`FslDemoService.FramePacingMarker.Impl`    | The service, its command line options, the run state machine and the overlay that draws the marker.

The hosts only use the Control interfaces: they get `IFramePacingMarkerServiceControl` with `TryGet` and create the overlay through it. The
service is only registered (by `FslDemoPlatform`) on the platforms that support the marker library, everywhere else `TryGet` returns
null and nothing is drawn.

The overlay draws the library's static grids: `MB::FramePacing::Marker::GridVertices` for the main and the sync marker, only regenerated
when the window size or the marker options change. Every frame it encodes the marker with `MB::FramePacing::Marker::GenerateModules` and
uploads just the 16-bit indices from `MB::FramePacing::Marker::ModulesToGridIndices` (about 5 KB). It renders on the GPU through the FslGraphics3D
`IBasicRenderSystem` (one dynamic vertex buffer, one dynamic index buffer, an opaque material without depth test or culling and a pixel
aligned orthographic projection), so the same code is used for OpenGL ES 2, OpenGL ES 3 and Vulkan. The host draws it inside the frame after the app has drawn (`DemoAppManager` for OpenGL ES and
`DemoAppVulkanBasic::AddSystemUI` for Vulkan).
