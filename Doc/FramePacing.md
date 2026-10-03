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
run id, intended display time, target frame time, CPU start time, CPU busy, preferred frame time, the static flags, and for start
markers the run start time and sequence id (`FramePacingSequenceId`). Times use `TimeSpan`/`TickCount`, and a value that is unknown
(the intended display time, target frame time and preferred frame time, unless the app supplies them) is an empty `std::optional`. The
marker is drawn at the end of the frame, so during a frame this is the previous frame's marker.

The framework has no frame pacer. An app with its own frame pacer calls `SetFrameSchedule(FramePacingFrameSchedule)` during its draw,
before the marker is drawn (on Vulkan before `AddSystemUI`), to supply the values it paced the frame by: the animation time, and
optionally the CPU start time, intended display time, target frame time, preferred frame time and the static after flag. It applies to that
frame only, a frame without the call reports the framework's values.

See the [GLES2.FramePacing](../DemoApps/GLES2/FramePacing), [GLES3.FramePacing](../DemoApps/GLES3/FramePacing) and
[Vulkan.FramePacing](../DemoApps/Vulkan/FramePacing) samples (they share their code in [Shared/FramePacing](../DemoApps/Shared/FramePacing)):
they show every value of the last marker in a panel, formatted into a reused `fmt::memory_buffer` so updating it every frame does not
allocate. `--HideMarkerStats` starts with the panel hidden.

## What the marker reports

- **Frame index**: a 64 bit counter that increments for every frame the app draws.
- **Animation time**: `FrameInfo::Time.CurrentTickCount`, the time the app's animation was evaluated for (100ns ticks). Fixed time
  step, pause and forced update time modes are reported exactly as the app sees them. An app that supplies a frame schedule reports
  the animation time of the schedule instead.
- **Intended display time, target frame time and preferred frame time**: reported as unknown (0) as the framework has no frame pacer, so
  the analysis measures every frame against the display's refresh rate (or the `--target-fps` given to the analysis). An app with its
  own frame pacer supplies them with `SetFrameSchedule`.
- **Static flags**: a frame that has the animation time of the frame before it (the app is paused, or it only animates on demand) is flagged
  as *static before*: nothing animated while the frame before it was on screen. The analysis then does not judge that step as a animation
  error, and leaves it out of the average fps, the 1 % and 0.1 % lows and the display time step statistics. The service does this by itself
  for every app. *Static after* (nothing animates while this frame is on screen) is only set if the app supplies it with `SetFrameSchedule`,
  as the framework does not know what a app will do next.
- **CPU start time and CPU busy**: the CPU start time is taken by the host just before the app update of the frame, and CPU busy is
  the time from then until the marker is drawn (the last thing before the frame is presented). The analysis derives the frametime
  from the step between the CPU start times.
- **Sync marker**: the run id and the frame index, so the analysis can match it to the main marker of the same run.

## The frame pacer of the FramePacing samples (experimental)

The framework has no frame pacer, but the FramePacing samples can pace their frames with the experimental frame pacer of the mb-framepacing
SDK (`MB::FramePacing::Pacer::FramePacer`). It is only part of the samples (`SamplePacer` in
[Shared/FramePacing](../DemoApps/Shared/FramePacing)), as mb-framepacing has only checked the pacer against its own simulation, never
against a real swap chain, and its API may change in any release.

The pacer needs nothing but a steady clock and a present that waits for vsync. Every frame the sample gives it the time the frame starts
and gets back the swap interval to hold the frame for (the number of display refreshes), the time step to animate the frame by and the
pacing values of the marker. The sample hands those to the marker with `SetFrameSchedule`, so with the pacer on the marker reports the
intended display time, the target frame time and the preferred frame time.

Control                       |Argument                        |Description
------------------------------|--------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Frame pacer (or the **P** key)|`--Pacer`                       |Switch the frame pacer on and off.
Refresh rate                  |`--Pacer.RefreshRate <hz>`      |The refresh rate of the display. It is read from the window system, the slider only sets it when the window system does not know it. The argument overrides both and allows decimals (59.94).
Target fps                    |`--Pacer.TargetFps <fps>`       |The frame rate the pacer aims for, 0 is the refresh rate of the display. 30 on a 60 Hz display holds every frame for two refreshes.
Adaptive swap interval        |`--Pacer.Adaptive <true\|false>`|On: the pacer slows down when frames are late and speeds up again when they fit. Off: a fixed frame rate.
CPU load                      |`--CpuLoad <ms>`                |The time in milliseconds the app spends busy every frame.
GPU load                      |`--GpuLoad <steps>`             |Draws the raymarched background with the given number of steps for every ray (0 is no background, the default is a low load of 16). The load grows linearly with the steps, more steps reach further and show finer detail.
Background                    |`--Background <flight\|hall>`   |The scene of the raymarched background (the radio buttons below the GPU load). `flight` is a flight through a fractal lattice. `hall` is a hall of columns that scrolls sideways at a constant speed, which makes a stutter easy to see.
Measure the presents          |                                |Vulkan only. Measure when the frames reach the display (`VK_EXT_present_timing`), see [below](#what-the-vulkan-sample-measures-about-its-presents). Start with `--VkPresentTiming false` to run without the extension.
Place the GPU work in time    |                                |Vulkan only. Show when the GPU worked on a frame, counted from the start of the frame (`VK_KHR_calibrated_timestamps`).
                              |`--PresentLog <file>`           |Vulkan only. Write a CSV file with one row per presented frame when the sample exits, see [below](#what-the-vulkan-sample-measures-about-its-presents).

The target fps slider and the adaptive switch can only be changed while the frame pacer is on. The line below the refresh rate shows the
rate the frames are paced at: the refresh rate divided by the swap interval. The two status lines below the switches show the swap interval
and the frame time that was measured, with how many of the last frames were late. They are shown with the pacer off as well: every frame is
then held for one refresh, and the late frames are the ones of the last two seconds that took more than one refresh.

Two overlays at the top right can be switched on and off (`Show the last marker` and `Show the frame pacing`, or start without them with
`--HideMarkerStats` and `--HidePacingStats`). The first shows every value of the last marker. The second shows what the frame pacing does:
the swap interval (and the one of the target frame rate), the average frame time of the last two seconds with the shortest and the longest
one, the late frames, the work of the last frame (the CPU time, and the GPU time if the app measures it), the average work the pacer decides
on as a share of the frame time, how long the present was delayed, how often the pacer made the swap interval longer (slower) or shorter
(faster) and when it last did, and the time its frame window spans. The values only the pacer has show `pacer off` while it is off. The
controls at the right can be scrolled if the window is too low for them.

The chart at the bottom shows the work of every frame: the CPU time and on top of it the GPU time, which together are the work the pacer is
told the frame needed (`Show the work chart`, or start without it with `--HideWorkChart`). The Vulkan sample measures the GPU time with
timestamp queries. The OpenGL ES samples have no such measurement: their GPU time is the time they wait in `glFinish`, which they only do
while the pacer is on, so with the pacer off their chart only shows the CPU time. The moving bar and box (the test pattern) can be switched
off as well (`Show the test pattern`, `--HideTestPattern`), and the sync marker at the bottom left can be switched on (`Draw the sync
marker`, or start with it with `--FramePacing.SyncMarker`).

```bash
# 30 fps on any display, with a GPU load
Vulkan.FramePacing --Pacer --Pacer.TargetFps 30 --GpuLoad 96
# The display rate until 20 ms of CPU load make the pacer slow down
GLES3.FramePacing --Pacer --CpuLoad 20
```

How a frame is held for its swap interval depends on the API:

- **OpenGL ES 2 and 3**: `eglSwapInterval`, so the swap of the host waits for the display. A EGL config only supports a range of swap
  intervals (it can be as short as one refresh), the sample holds the frame for the rest by delaying the swap. While the pacer is on the
  sample calls `glFinish` before the swap, so the time the pacer is told the frame needed includes the GPU work.
- **Vulkan**: a FIFO present holds a frame for one refresh and there is no swap interval, so the sample delays the present of a frame
  that is held longer: it waits until one refresh before the time the pacer aims the frame at, then lets the host present it. The pacer
  has no vsync times, so this is a guess and less even than a real swap interval. The GPU time of a frame is measured with timestamp
  queries and given to the pacer. The sample can measure when its frames reach the display (see the next section), but the pacer has no
  input for that, so the measurements are only shown.

### What the Vulkan sample measures about its presents

The Vulkan sample can show what happened to a frame after it was presented. Both measurements are optional: they use extensions the device
may not have, each has a switch so what it adds can be seen, and their rows of the frame pacing overlay show `not supported` without the
extension and `switched off` while the switch is off. Nothing else in the sample depends on them.

- **`Measure the presents`** (`VK_EXT_present_timing`, which needs `VK_KHR_present_id2` and `VK_KHR_calibrated_timestamps`): the swapchain
  reports for every present when it was handed to the presentation engine and when its first pixel left for the display. A report arrives
  a few frames after its present. Present timing is a property of the swapchain, so the switch recreates the swapchain. The rows:
  - `Display error`: the time from the display time the pacer aimed for to the measured one, the average and the worst of the last frames.
  - `Display interval`: the time between two frames in a row reaching the display, the average with the shortest and the longest one. This
    is how even the frames really are.
  - `Latency`: the time from the start of a frame to it reaching the display, and how long after the start it was handed over.
  - `Timed frames`: how many of the measured frames have a display time. The presentation engine does not have one for every frame, which
    does not mean that the frame was not shown.
  - `Display refresh`: the duration of a refresh according to the swapchain. It is only shown: the pacer keeps the refresh rate of the
    window system, as the one of the swapchain was seen to change between runs on a display that did not change.
- **`Place the GPU work in time`** (`VK_KHR_calibrated_timestamps`): the `GPU work` row shows when the GPU started and when it finished
  the frame, counted from when the CPU started on it. The timestamp queries of the GPU time are converted to the clock of the CPU for it.

Read the display times as what the driver reports, not as a measurement of the display, and switch variable refresh (G-SYNC, FreeSync)
off before trusting them. On the one system this was checked on (a NVIDIA desktop GPU with driver 617.14 and a window on the Windows 11
desktop) the display times were one refresh apart and after their present with G-SYNC off. With G-SYNC on the swapchain still reported a
fixed refresh mode, but the display times of frames whose present the sample delays were on no refresh grid and often before the present
call. The frame pacer needs a fixed refresh rate as well. A capture of the marker is the measurement, these rows are a quick look.

The `Display error` also shows how far the model of the pacer is from a Vulkan swapchain. The pacer takes a frame to be shown one swap
interval after it started, while a FIFO swapchain has a number of presents queued between the app and the display.

`--PresentLog <file>` writes one row per presented frame to a CSV file when the sample exits, so the frame loop can be looked at
afterwards. Every time is a time of the steady clock of the framework (HighResolutionTimer) in whole 100ns ticks, and a field is empty
when the value is not available.

Column                                  |Description
----------------------------------------|----------------------------------------------------------------------------------------------
`frameIndex`                            |The frame index the frame pacing marker of the frame carried, so a row can be matched to a frame of a capture.
`presentId`                             |The number of the present (from one).
`cpuStartTicks`                         |When the sample started on the frame (after the swapchain image was acquired).
`endFrameTicks`                         |When the work of the frame was done, which is what the pacer is told.
`acquireCallTicks`, `acquireReturnTicks`|When `vkAcquireNextImageKHR` was called and when it returned.
`presentCallTicks`, `presentReturnTicks`|When `vkQueuePresentKHR` was called and when it returned.
`queueOperationsEndTicks`               |When the present was handed to the presentation engine (`VK_EXT_present_timing`).
`firstPixelOutTicks`                    |When the frame reached the display (`VK_EXT_present_timing`).
`refreshDurationNs`                     |The duration of a refresh in nanoseconds according to the swapchain.
`swapInterval`, `intendedDisplayTicks`  |What the pacer planned for the frame (empty while it is off).
`animationTimeTicks`                    |The time the frame was animated for.
`change`                                |What the pacer did to the swap interval at this frame: `unchanged`, `slower` or `faster` (empty while it is off).
`imageIndex`                            |The swapchain image that was presented.
`resultReadAtPresentId`                 |The present of the frame in which the measurement of this present was read: how late it arrived.

```bash
Vulkan.FramePacing --Pacer --PresentLog frames.csv --ExitAfterFrame 2000
```

The GPU load is a raymarched background with two scenes, selected with the radio buttons below the GPU load or with `--Background`. `Fractal
flight` is a flight through a fractal lattice of golden spheres over water that mirrors it (a sphere inversion fractal). `Scrolling hall` is
a hall of fluted columns on a mirroring floor at dusk: the camera only travels sideways, at a constant speed, so every column, shadow and
tile crosses the screen at a constant speed and a frame that is shown too long or too short is easy to see. In both scenes every ray is
sphere traced in a fixed number of steps without a early exit, so every pixel costs the same and the cost grows linearly with the steps.
Each app has its own copy of the shader (`Raymarch.frag`), as the shared code only knows the API independent render interfaces.

While the pacer is on the sample animates by the time steps of the pacer (the refreshes the display moved on), so the time step keys of
the framework (slow and fast motion) have no effect. Pause still stops the animation.

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
`ThirdParty/mb_framepacing`                | The mb-framepacing C++ SDK: its marker module and, for the samples, its experimental pacer module (via `Recipe.mb_framepacing_0_1`).
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
