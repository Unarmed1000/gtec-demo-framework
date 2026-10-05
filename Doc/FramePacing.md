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
`--FramePacing.Log <file>`        | Write what every frame did to a log, see [the frame log](#the-frame-log). It does not need the marker to be drawn.

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

The recipe pins a commit of mb-framepacing. The pacer of that commit counts a frame whose work is longer than its frame time as late,
whenever the next frame starts. It can be given when the display showed the frames (present feedback, see below), which it counts
and does not pace by.

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
Present feedback to the pacer |`--Pacer.PresentFeedback <true\|false>`|Vulkan only. On: the pacer is given when the display showed the frames and counts what the display did, see [below](#present-feedback-vulkan-optional). It paces the same on and off. Off is the default.
Hold (radio buttons)          |`--Pacer.Hold <auto\|vsync\|wait\|schedule>`|Vulkan only. How a frame is held for more than one refresh, see [below](#holding-a-frame-for-more-than-one-refresh-vulkan). The default is `wait`.
                              |`--Pacer.VSyncPhase <percent>`  |Vulkan only. For the `vsync` hold: where in the refresh before the target the present is done (1 to 99, the default is 65).
                              |`--Pacer.Profile <late\|early\|off>`|Vulkan only. Where a frame waits for the time the pacer gives for the next frame, see [below](#waiting-for-the-time-of-the-pacer-vulkan). The default is `early`.
                              |`--Pacer.Drain <refreshes>`     |Vulkan only. A wait of that many refreshes, once, half a second after the sample began to wait for the time of the pacer (0 to 32, the default is 4, 0 is none).
CPU load                      |`--CpuLoad <ms>`                |The time in milliseconds the app spends busy every frame.
GPU load                      |`--GpuLoad <steps>`             |Draws the background with the given load (0 is no background, the default is a low load of 16). The cost grows linearly with it. It is the number of steps of every ray, more steps reach further; for the lace every doubling of it adds a round of finer detail and the rest of it is samples per pixel.
Background                    |`--Background <name>`           |The scene of the background (the radio buttons below the GPU load). `blobs` (the default) is a flight through blobs that melt into each other and `lace` is circles packed into circles: both are cheap at a low load. `flight` is a raymarched flight through a fractal lattice and `hall` a raymarched hall of columns that scrolls sideways at a constant speed, which makes a stutter easy to see: both cost a lot from the first step on.
Background resolution         |`--BackgroundScale <percent>`   |The resolution the background is drawn at, in percent of the resolution of the window (10-100, the default is 100). Below 100 it is drawn into a smaller picture that is enlarged to the window, so its cost falls with the number of pixels: for a GPU that is limited by the pixels. The UI, the marker and the test pattern stay sharp.
Measure the presents          |                                |Vulkan only. Measure when the frames reach the display (`VK_EXT_present_timing`), see [below](#what-the-vulkan-sample-measures-about-its-presents). Start with `--VkPresentTiming false` to run without the extension.
Place the GPU work in time    |                                |Vulkan only. Show when the GPU worked on a frame, counted from the start of the frame (`VK_KHR_calibrated_timestamps`).

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
timestamp queries. The OpenGL ES samples measure it with a time elapsed query (`GL_EXT_disjoint_timer_query`), with the pacer on or off.
None of the samples waits for the GPU to measure it, so the GPU time is the one of an earlier frame. Without the extension the OpenGL ES
samples have no GPU time and their chart only shows the CPU time. The moving bar and box (the test pattern) can be switched
off as well (`Show the test pattern`, `--HideTestPattern`), and the sync marker at the bottom left can be switched on (`Draw the sync
marker`, or start with it with `--FramePacing.SyncMarker`).

The box animation of the mb-framepacing-explained videos can be shown as well (`Show the box animation`, `Fast box animation`, or start with
it with `--BoxAnimation normal` or `--BoxAnimation fast`; it is off by default). It is a white box that eases from side to side with a
short rest at both ends, in the part of the window that is left of the controls: a round trip takes four seconds, two when fast. The box is
drawn where the animation time of the frame puts it, exactly as the test pattern is, so the motion on the display can be compared by eye
with what those videos show for a steady frame rate, a wrong animation step or a frame that is shown late. Only the box is drawn, on top of
the background and of the test pattern: for a clear view of it switch the test pattern and the two overlays off (`--HideTestPattern
--HideMarkerStats --HidePacingStats`).

```bash
# 30 fps on any display, with a GPU load
Vulkan.FramePacing --Pacer --Pacer.TargetFps 30 --GpuLoad 96
# The display rate until 20 ms of CPU load make the pacer slow down
GLES3.FramePacing --Pacer --CpuLoad 20
```

How a frame is held for its swap interval depends on the API:

- **OpenGL ES 2 and 3**: `eglSwapInterval`, so the swap of the host waits for the display. A EGL config only supports a range of swap
  intervals (it can be as short as one refresh), the sample holds the frame for the rest by delaying the swap. The GPU time of a frame
  is measured with a time elapsed query (`GL_EXT_disjoint_timer_query`) and given to the pacer, the sample does not call `glFinish`.
  Without the extension the pacer is told the CPU time only.
- **Vulkan**: a FIFO present holds a frame for one refresh and there is no swap interval, so the sample delays the present of a frame
  that is held longer: it waits until one refresh before the time the pacer aims the frame at, then lets the host present it. The pacer
  has no vsync times, so this is a guess and less even than a real swap interval. The GPU time of a frame is measured with timestamp
  queries and given to the pacer. The sample can measure when its frames reach the display (see the next section) and, when asked to,
  gives those display times to the pacer as present feedback.

#### Waiting for the time of the pacer (Vulkan)

The pacer gives every frame the time the next frame may start (`NextFrameStartTime`: the start of the frame plus its swap interval),
and the app is the one that has to wait for it. That holds for every frame, also one that is shown for a single refresh: a FIFO
present was expected to make the loop wait for the display there and does not. `vkAcquireNextImageKHR` and `vkQueuePresentKHR`
were measured to return at once, the loop then only waits for the GPU to finish the frame before, and with work close to a refresh
it makes more frames than the display shows. Until this was found the sample only waited for a frame that is held for two or more
refreshes, so every capture of a frame at a swap interval of one before it is of a loop that did not apply the time of the pacer.

`--Pacer.Profile` is which side of the present the wait is on. A frame is started once per swap interval with both:

- **`early`** (the default): the frame is rendered right away and its present waits, so a present can not come before the display
  has taken the one before it. The next frame starts when the present was made. A frame that is done early waits finished, so it is
  older when it is shown, and when its present is made does not depend on the work.
- **`late`**: the start of the frame waits and the frame is presented when it is done. The frame is as fresh as it can be when it
  is shown. When its present is made moves with how long the work took.
- **`off`**: no wait. It is not frame pacing, it is there so a capture can show the loop without it.

The times are kept one frame time apart, counted from the time the frame before was held to and not from where the loop is now: the
loop takes time to get from a present to the start of the next frame, and that would be added to every frame.

What the wait is for follows `--Pacer.Hold`: a time on the clock of the app (`wait`, the default), or the vsync of the window system
(`vsync`: a present is placed at `--Pacer.VSyncPhase` of the refresh, the start of a `late` frame on a vertical blank).

First runs on Windows at 240 Hz (variable refresh off and read during every run, one run of 2400 frames each, a window):

| | Light work: presents never shown, frames shown too long | GPU work of 82 to 87 % of a refresh |
|---|---|---|
| `off` | 3, 0 | 116, 146 |
| `early`, clock | 1, 1 | 2, 2 |
| `late`, clock | 3, 3 | 7, 12 |
| `early`, vsync | 1, 2 | 9, 12 |
| `late`, vsync | 3, 2 | 34, 67 |

So waiting for the time of the pacer is what stops the loop from making frames the display never shows, with either profile. The
wait on the vsync was not better than the clock in these runs. `late` on the vsync is not a good pair: the GPU starts a frame at a
fixed place in the refresh (when the swapchain image is released) and the loop waits for it to finish, so a start that is asked for
at the vertical blank comes late and the next one early (frame starts 2.8 to 5.5 ms apart). `early` with two frames in flight had 5
presents never shown on the clock and 144 on the vsync. One run each: the order among the good ones says nothing yet.

Waiting alone does not shorten the way from a present to the display. A present reached the display 10 to 15 ms after its call in
the runs with light work (about three refreshes), with every profile; in a loop that does not wait it is 15.3 ms. A trace of the
presents (PresentMon) shows where that time is on Windows with a NVIDIA driver, which runs the Vulkan swapchain on a DXGI swapchain:
the first dozen presents of a window go through the compositor and take three refreshes from the present of the driver to the
display, and the presents the app makes meanwhile queue up in the driver. Then the window is flipped directly, the present of the
driver reaches the display in one refresh (4.0 ms), and the app's presents still wait 5.6 to 7.2 ms in front of it: a loop that
makes one frame per refresh does not work a queue off.

`--Pacer.Drain <refreshes>` is for that: half a second after the sample began to wait for the time of the pacer it waits that
many refreshes more, once, so what is queued is shown. With the default of four a present reached the display after 2.8 ms from
that wait to the end of the run, where it was 11.1 ms before it and 9.7 ms for a whole run without it (light work, one run each).
With GPU work of 86 % of a refresh it went from 8.0 to 3.9 ms and was back at 8.1 ms nine frames later, one refresh behind again,
and stayed there: a frame that is late once adds to the queue and nothing takes it off. An app that knows when its frames are shown
(`VK_EXT_present_timing`) could wait again when that happens; the sample does not yet.

`late` on the clock is not safe either: in one run its frame starts were 4.166 to 4.167 ms apart, in another 2.9 to 5.5 ms with 20
presents never shown. Where its start lands in the refresh is chance, and the place before the GPU is done with the frame before
is a bad one. `early` does not have this, its frame starts follow its presents.

A frame that is held for more refreshes than the present holds it for is the next section.

#### Holding a frame for more than one refresh (Vulkan)

Core Vulkan has no swap interval: a FIFO present shows a frame at the next refresh, and nothing in Vulkan says when a refresh
happens. So a frame the pacer holds for more than one refresh has to be held by something else, and the sample has three ways
(`--Pacer.Hold`, the `Hold` radio buttons). [FramePacingPlatformSupport.md](FramePacingPlatformSupport.md) lists what each platform
offers for them. A radio button of a way the system can not do is disabled, as every control of the sample that can not be used is
(none is hidden): `schedule` without a swapchain that takes a target time, `vsync` while the window system gives no vsync time. All of
them are disabled while the pacer is off and in the OpenGL ES samples, where `eglSwapInterval` holds the frame.

- **`wait`: the sample sleeps on a timer, then presents** (the default, it needs nothing). It sleeps until one refresh before the time
  the frame is aimed at. The timer does not know where the refreshes are, so it is a guess: when a present lands near a vertical
  blank, a frame is shown a refresh too early and its neighbour a refresh too long. How often depends on where the timer happens to
  start and on the refresh rate: next to never at 50, 60 and 120 Hz, and from none to 35 % of the frames at 240 Hz in the runs that
  were measured.
- **`vsync`: the sample waits on the vsync of the window system, then presents.** It needs no Vulkan extension, only a window system
  that says when the display refreshes (`INativeWindow::TryGetVSyncInfo`: Windows, Wayland with presentation-time, X11 with Present and Android from API level 33; `--VSyncSource`
  selects where the window takes the time from, see [FramePacingPlatformSupport.md](FramePacingPlatformSupport.md)). The frame is aimed at the vertical blank
  nearest to its start plus its swap interval and presented inside the refresh before that one, and the next frame starts at the
  target. `--Pacer.VSyncPhase` is where in that refresh the present is done, in percent: only a part of a refresh is safe, and where
  that part is has to be measured for a platform (55 to 75 % on Windows at 240 Hz, nearly all of the refresh at 120 Hz and below, the
  default is 65). It is not for a display with a variable refresh rate (G-SYNC, FreeSync): the vertical blank follows the frames
  there, and a frame held by it was shown for anything from one to seven refreshes. The sample stops using it when it sees that
  (see below).
- **`schedule`: the presentation engine holds the frame.** The present is given a target time and is done right away
  (`VK_EXT_present_timing` where the device and the surface have `presentAtRelativeTime`): the image is not shown before the target
  time has passed since the image of the present before it was shown, and then at the first refresh. The sample asks for the swap
  interval minus half a refresh.
- **`auto`**: `schedule` if the swapchain can do it, else `vsync` if the window system reports its vsync, else `wait`.

A method the system can not do falls back to `vsync`, else `wait`. The frame log says what every frame used (`holdMethod`), where the
vsync wait aimed (`holdTargetTicks`), what the sample asked a scheduled present for (`presentTargetTicks`) and what the present was
given (`presentTargetRelativeNs`). The `presentTiming` event has `canSchedule=1` where a present can take a target time.

The vsync wait is also left when the display was seen to refresh at a variable rate: the window says so
(`INativeWindow::TryGetVariableRefreshInfo`, on Windows a measurement of the vertical blanks) or the swapchain does. From then on the
frames are held with the sleep and `Hold: wait on the vsync` is shown as disabled, until another hold is selected or the refresh rate
of the display changes, so the method does not go back and forth. The `Variable refresh` row of the frame pacing overlay shows what is
known: `not seen`, `seen` with the refreshes of the mode between two refreshes of the display, `seen before`, or `unknown`, and in
the Vulkan sample what the swapchain says (`swapchain: fixed`). `not seen` is no proof that variable refresh is off: a display with
it on refreshes like a fixed one while the frames come at the rate of its mode. The frame log has the event `holdVariableRefresh`
where the sample left the vsync wait. What a platform can tell is in
[FramePacingPlatformSupport.md](FramePacingPlatformSupport.md#variable-refresh-what-a-platform-tells-an-app).

What the frame loop of a Vulkan app waits on, and why the hold is the sample's to do: a FIFO present returns at once, the acquire
returns at once, and the loop only waits for the GPU to finish the frame before (the fence of its queue submit). With light work the
display still paces the loop through the swapchain image the submit waits for. With work close to a refresh it does not: the loop
runs at the speed of the GPU, a little faster than the display, and presents are dropped (97 of 2340 at 240 Hz with GPU work of
92 % of a refresh). The numbers are in [FramePacingCapture.md](FramePacingCapture.md).

Any Vulkan app can schedule a present: `DemoAppVulkanBasic::IsPresentSchedulingSupported()` and
`SetPresentRelativeTargetTime(time)` before the frame is presented. The absolute form of the extension (`presentAtAbsoluteTime`) is not
used.

#### Present feedback (Vulkan, optional)

`--Pacer.PresentFeedback true` (the `Present feedback to the pacer` switch) gives the pacer the time the display showed each frame.
It is statistics only: the pacer paces exactly as without it (the swap interval, the animation step, the start of the next frame and
the late frames of its frame window all come from the frame starts and the work of the frames). What feedback adds is the counters
of the pacer about the display and an intended display time that is counted from the display times. It needs the presents to be
measured (`VK_EXT_present_timing`, the `Measure the presents` switch), so it does nothing for the OpenGL ES samples, and it is off by
default.

Until mb-framepacing `19acdf7` the pacer decided on late frames by the display times. That was taken out with `b5b6ab3`.

On a display with a variable refresh rate (G-SYNC, FreeSync) the display times are on no grid of refreshes, the pacer refuses them
and the counters say nothing.

What the sample does with it:

- Every frame it remembers the id the pacer gave the frame next to the id of the present of the frame. When the display time of a
  present arrives, a few frames later, it gives the pacer the display time and the time `vkQueuePresentKHR` was called for that frame.
  A present the presentation engine is done with and has no display time for is reported to the pacer as not shown, which the
  pacer counts. A present that was not asked to be timed gets no feedback.
- The intended display time of the marker is then the refresh the frame reaches through the queue of the swapchain (`Display error`
  drops to about zero), and unknown for the first frames.
- The waits of the sample hold to the start of the frame plus its swap interval, with and without feedback.
- The `Present feedback` row of the frame pacing overlay shows how many display times the pacer used and refused and for how many
  frames it had none. The `Display late` row is how many refreshes the display fell behind the swap intervals of the frames, next to
  the `Late frames` the pacer counts from the frame starts. The frame log has it per frame
  (`pacerFeedbackOn`, `pacerFrameId`, `feedbackDisplayTicks`, `feedbackPresentTicks`, `feedbackNotShown`, `pacerFeedbackUsed`,
  `pacerFeedbackRefused`, `pacerFeedbackNotShown`, `pacerFeedbackMissing`, `pacerFeedbackLateRefreshes`).
  `pacerFeedbackLateRefreshes` is the count of the display to hold against `pacerWindowLateFrames`, the count the pacer paces by.

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
  - `Swapchain refresh`: the duration of a refresh according to the swapchain. It is only shown: the pacer keeps the refresh rate of the
    window system. The one of the swapchain is not always the one of the display the window is on: with two displays at different rates
    it was the one of the fastest display (120 Hz for a window on a display at 50 Hz), and the row then names the rate of the display
    as well.
- **`Place the GPU work in time`** (`VK_KHR_calibrated_timestamps`): the `GPU work` row shows when the GPU started and when it finished
  the frame, counted from when the CPU started on it. The timestamp queries of the GPU time are converted to the clock of the CPU for it.

Read the display times as what the driver reports, not as a measurement of the display, and switch variable refresh (G-SYNC, FreeSync)
off before trusting them. On the one system this was checked on (a NVIDIA desktop GPU with driver 617.14 and a window on the Windows 11
desktop) the display times were one refresh apart and after their present with G-SYNC off. With G-SYNC on the swapchain still reported a
fixed refresh mode, but the display times of frames whose present the sample delays were on no refresh grid and often before the present
call. The frame pacer needs a fixed refresh rate as well. A capture of the marker is the measurement, these rows are a quick look.

The `Display error` also shows how far the model of the pacer is from a Vulkan swapchain. The pacer takes a frame to be shown one swap
interval after it started, while a FIFO swapchain has a number of presents queued between the app and the display.

The GPU load is the background. It has four scenes, selected with the radio buttons below the GPU load or with `--Background`. `Blobs` (the
default) and `Lace` are cheap at a low load, so a slow GPU has a load that fits in a frame. `Blobs` is a flight through a field of blobs
that melt into each other: every ray is sphere traced in a fixed number of cheap steps, and more steps reach further. `Lace` is a lace of
circles packed into circles drawn with gold threads, which the animation zooms into and out of. Its load adds detail: every doubling of it
is one more round of finer circles (four rounds at the default load of 16, ten at 1024), and the rest of the load is the number of samples
every pixel is drawn with, which smooths the edges. The finest rounds are only drawn where they are large enough on the screen, so they
show when the animation is zoomed in.

The other two are raymarched and cost a lot from the first step on. `Fractal
flight` is a flight through a fractal lattice of golden spheres over water that mirrors it (a sphere inversion fractal). `Scrolling hall` is
a hall of fluted columns on a mirroring floor at dusk: the camera only travels sideways, at a constant speed, so every column, shadow and
tile crosses the screen at a constant speed and a frame that is shown too long or too short is easy to see. In these two scenes every ray is
sphere traced in a fixed number of steps without a early exit, so every pixel costs the same and the cost grows linearly with the steps.
Each app has its own copy of the shader (`Raymarch.frag`), as the shared code only knows the API independent render interfaces.

While the pacer is on the sample animates by the time steps of the pacer (the refreshes the display moved on), so the time step keys of
the framework (slow and fast motion) have no effect. Pause still stops the animation.

## The frame log

`--FramePacing.Log <file>` makes the service write what every frame of the app did to a log, so the frame loop can be looked at
afterwards and compared with a capture of the marker. It works with every OpenGL ES and Vulkan app and does not need the marker to be
drawn. How to capture runs that can be compared, with a plan, a known load on the machine and notes, is described in
[FramePacingCapture.md](FramePacingCapture.md).

```bash
Vulkan.FramePacing --Pacer --FramePacing.Log frames.csv --ExitAfterFrame 2000
```

Two files are written:

- **`<file>`**: one row per frame, the first line holds the names of the columns. A row is a frame the host began, in order, and
  `frameIndex` is the frame index the marker of the frame carries.
- **`<stem>.events.csv`** (`frames.events.csv`): `frameIndex,timeTicks,event,details` for everything that is not a value of a frame:
  the facts of the run, the description of every column and what happened during the run.

Every value of a frame is a whole number and nothing is rounded or converted to a unit with decimals. A time (`ticks`) is a time of the
steady clock of the framework (`HighResolutionTimer`) in 100 ns ticks and a duration (`durationTicks`) is in the same ticks. A value of
the driver is written as the driver gave it, with its unit in the name of the column (`refreshDurationNs`). A field is empty when the
frame has no such value: the app does not supply it, the extension is missing, or the value did not arrive.

The log is written by a thread of its own, the render thread only hands over rows. The files are flushed every 250 ms, so a app that
crashes or is killed loses less than a second. A row stays open for 64 frames, as the values of a frame arrive over time (the display
time of a present is reported a few frames later), and is written when it closes.

### The columns

Which columns a log has depends on the app: every app has the ones of the service and the host, a Vulkan app adds its presents and the
FramePacing samples add their pacer. The events file describes every column of the log it belongs to.

**The service, for every app**

Column | Unit | Description
---|---|---
`markerKind` | code | The kind of marker of the frame: 0 frame, 1 the start of a run, 2 the end of a run
`runId` | id | The id of the current (or last) run, as the marker carries it
`runState` | code | The state of the run: 0 idle, 1 starting, 2 measuring, 3 ending
`animationTimeTicks` | durationTicks | The time the frame is animated for, as the marker carries it (the one of the app if it gave a schedule)
`cpuStartTicks` | ticks | When the CPU started on the frame, as the marker carries it (the one of the app if it gave a schedule)
`hostCpuStartTicks` | ticks | When the host started the update of the frame
`beginFrameTicks` | ticks | When the host told the service the frame begins: after the update and after the frame was prepared for drawing
`hasSchedule` | flag | 1 if the app gave the pacing values of the frame
`intendedDisplayTicks` | ticks | When the frame pacer of the app intends the frame to be shown
`targetFrameTimeTicks` | durationTicks | The frame time the frame pacer of the app aims for
`preferredFrameTimeTicks` | durationTicks | The frame time the app wants to run at
`markerDrawn` | flag | 1 if the marker was drawn on the frame
`markerDrawTicks` | ticks | When the marker was drawn, the last thing of the frame
`markerCpuBusyTicks` | durationTicks | How long the CPU worked on the frame before the marker was drawn, as the marker carries it
`markerStatic` | flag | The static after flag of the marker
`markerStaticBefore` | flag | The static before flag of the marker
`markerSyncMarker` | flag | 1 if the sync marker was drawn as well
`markerModuleSizePx` | pixels | The size of a module of the marker that was asked for

**The load of the machine, written once a second and empty in between**

Column | Unit | Description
---|---|---
`systemIdleTicks` | durationTicks | How long the CPUs of the system were idle since it started, summed over the CPUs
`systemKernelTicks` | durationTicks | How long the CPUs of the system ran kernel code since it started, idle time not included
`systemUserTicks` | durationTicks | How long the CPUs of the system ran user code since it started
`processKernelTicks` | durationTicks | How long this process ran kernel code since it started
`processUserTicks` | durationTicks | How long this process ran user code since it started
`processGpuUsageMilliPercent` | count | The load of the busiest GPU engine this process uses in thousandths of a percent
`processGpuDedicatedBytes` | count | The memory of the GPU this process uses in bytes
`processGpuSharedBytes` | count | The system memory the GPU uses for this process in bytes

**The host, for every app**

Column | Unit | Description
---|---|---
`hostUpdateEndTicks` | ticks | When the update of the app was done
`hostDrawEndTicks` | ticks | When the draw of the app was done
`hostSwapCallTicks` | ticks | When the host started to swap the frame (or asked the app to present it)
`hostSwapReturnTicks` | ticks | When the swap returned
`hostSwapCompletedTicks` | ticks | When the host was done with the frame, after the swap
`hostFrameSlot` | id | The frame slot of the render loop the frame used (the frames in flight)
`frameworkTimeTicks` | durationTicks | The time of the framework the frame was updated and drawn for
`frameworkStepTicks` | durationTicks | The time step of the framework from the frame before
`displayVSyncTicks` | ticks | The time of a recent vertical blank of the display as the window system reported it when the frame began (empty if the platform does not report it)
`displayRefreshPeriodTicks` | durationTicks | The time between two refreshes of the display as the window system measured it, read with displayVSyncTicks
`displayVSyncFlags` | code | What the window system says about how displayVSyncTicks was obtained, zero where it says nothing. Wayland: the kind flags of presentation-time (1 in sync with the display, 2 a time of the display hardware, 4 the hardware signalled the frame was shown, 8 zero copy)
`displayVBlankIntervalMilliPeriods` | count | The median time between the last vertical blanks of the display in thousandths of the refresh period of its mode: 1000 is a display that refreshes at the rate of its mode, 2000 one that refreshes every second period (variable refresh at half the rate). Empty if the platform does not measure it (Windows does)
`displayVBlankOffPeriodPerMille` | count | The share of those vertical blanks that did not come one refresh period after the one before, in thousandths, read with displayVBlankIntervalMilliPeriods

**Vulkan apps (`DemoAppVulkanBasic`)**

Column | Unit | Description
---|---|---
`presentId` | id | The number of the present of the frame, counted from one
`imageIndex` | id | The index of the swapchain image that was presented
`swapchainGeneration` | count | How many swapchains were created up to this frame
`acquireCallTicks` | ticks | When vkAcquireNextImageKHR was called
`acquireReturnTicks` | ticks | When vkAcquireNextImageKHR returned
`presentCallTicks` | ticks | When vkQueuePresentKHR was called
`presentReturnTicks` | ticks | When vkQueuePresentKHR returned
`presentResult` | code | The VkResult of vkQueuePresentKHR
`acquireResult` | code | The VkResult of vkAcquireNextImageKHR (zero is success, 1000001003 is VK_SUBOPTIMAL_KHR: the image was acquired, the swapchain no longer matches the surface exactly)
`presentTimingRequested` | flag | 1 if the present was asked to be timed, 0 if not: present timing is off, or too many results were outstanding
`refreshDurationNs` | nanoseconds | VkSwapchainTimingPropertiesEXT::refreshDuration as the swapchain last reported it
`refreshIntervalNs` | nanoseconds | VkSwapchainTimingPropertiesEXT::refreshInterval as the swapchain last reported it
`queueOperationsEndTicks` | ticks | When the queue operations of the present ended (the image was handed to the presentation engine), on the clock of the framework
`queueOperationsEndRawNs` | nanoseconds | When the queue operations of the present ended (the image was handed to the presentation engine), as the presentation engine reported it on the clock of its time domain
`requestDequeuedTicks` | ticks | When the presentation engine took the present from its queue, on the clock of the framework
`requestDequeuedRawNs` | nanoseconds | When the presentation engine took the present from its queue, as the presentation engine reported it on the clock of its time domain
`firstPixelOutTicks` | ticks | When the first pixel of the image left for the display, on the clock of the framework
`firstPixelOutRawNs` | nanoseconds | When the first pixel of the image left for the display, as the presentation engine reported it on the clock of its time domain
`firstPixelVisibleTicks` | ticks | When the first pixel of the image became visible on the display, on the clock of the framework
`firstPixelVisibleRawNs` | nanoseconds | When the first pixel of the image became visible on the display, as the presentation engine reported it on the clock of its time domain
`presentTimeDomainId` | id | The id of the time domain the stages were reported in
`resultReadAtFrame` | id | The frame in which the stages of this frame were read: how late they arrived
`presentTimingRequested` | flag | 1 if the present was asked to be timed, 0 if not: present timing is off, or too many results were outstanding
`presentTargetRelativeNs` | nanoseconds | The target time the present was given: its image is not shown before this long after the image of the present before it was shown (empty: the present was not scheduled)

**The FramePacing samples**

Column | Unit | Description
---|---|---
`pacerOn` | flag | 1 if the frame pacer of the sample paced the frame
`swapInterval` | count | The number of display refreshes the frame pacer holds the frame for
`preferredSwapInterval` | count | The swap interval of the target frame rate, the pacer never runs faster
`pacerChange` | code | What the frame pacer did to the swap interval at this frame: 0 unchanged, 1 slower, 2 faster
`animationStepTicks` | durationTicks | The step of the frame pacer from the animation time of the frame before
`pacerWindowFrames` | count | The frames in the frame window of the frame pacer
`pacerWindowLateFrames` | count | The frames in the frame window the frame pacer counts as late
`pacerWindowStartsAheadTicks` | durationTicks | How far the frames in the frame window began before the times the frame pacer gave for them (`NextFrameStartTime`), added up over the frames that were not late. About zero: the app waits for those times. Positive: the loop runs ahead of them (more than a refresh per second of the window: nothing holds it to the display). Negative: it is behind, the work does not fit
`pacerWindowAverageWorkTicks` | durationTicks | The average work of the frames in the frame window of the frame pacer
`pacerWindowSpanTicks` | durationTicks | The time the frame window of the frame pacer spans
`pacerWindowFull` | flag | 1 if the frame window of the frame pacer is full
`frameWaitStartTicks` | ticks | When the sample began to wait for the start of the frame (it holds the start to the time the frame before was aimed at)
`frameStartTicks` | ticks | When the sample started the frame, which is the start the frame pacer is given
`endFrameTicks` | ticks | When the work of the frame was done, which is what the frame pacer is told
`workCpuTicks` | durationTicks | How long the CPU worked on the frame
`workGpuTicks` | durationTicks | The GPU time the frame pacer was told with the frame: the one of the last frame that was measured
`gpuWorkBeginTicks` | ticks | When the GPU started on the frame, on the clock of the framework
`gpuWorkEndTicks` | ticks | When the GPU finished the frame, on the clock of the framework
`presentWaitTicks` | durationTicks | How long the sample delayed the present of the frame, to hold it for its swap interval
`cpuLoadMs` | count | The CPU load setting: the milliseconds the sample is busy per frame
`gpuLoadSteps` | count | The GPU load setting: the steps of the background (for the lace the rounds of detail and the samples per pixel come from it)
`holdMethod` | code | How the frame is held for more than one refresh: 0 the sample sleeps on a timer, 1 it waits on the vsync of the window system, 3 the present has a target time
`holdTargetTicks` | ticks | The vertical blank the frame was aimed at when it was held by waiting on the vsync
`presentTargetTicks` | durationTicks | The target time the sample asked for: the frame is not to be shown before this long after the frame before it was shown
`pacerFrameId` | id | The id the frame pacer gave the frame, present feedback is given with it
`nextFrameStartTicks` | ticks | The start of the frame plus its swap interval according to the frame pacer: what the waits of the sample hold to
`pacerFeedbackOn` | flag | 1 if the frame pacer is given the display times of the frames: it counts what the display did, it paces the same
`feedbackDisplayTicks` | ticks | The display time of the frame the frame pacer was given as present feedback
`feedbackPresentTicks` | ticks | The present time of the frame the frame pacer was given with its display time
`feedbackNotShown` | flag | 1 if the frame was reported to the frame pacer as not shown: the presentation engine was done with its present and had no display time for it
`pacerFeedbackUsed` | count | The display times the frame pacer counted from, counted since the pacer was made
`pacerFeedbackRefused` | count | The display times the frame pacer refused, counted since the pacer was made: too old, before the present of their frame or not a whole number of refreshes after the one before
`pacerFeedbackNotShown` | count | The frames that were reported to the frame pacer as never shown, counted since the pacer was made
`pacerFeedbackMissing` | count | The frames the frame pacer was given nothing about, counted since the pacer was made: feedback for a newer frame came first, or the frame got too old
`pacerFeedbackLateRefreshes` | count | The refreshes the display fell behind the swap intervals of the frames by its display times, counted since the pacer was made: the count of the display to hold against the late frames of the frame pacer

`presentTimingRequested`, `resultReadAtFrame` and the stage times tell three cases apart: a present that was not asked to be timed
(the results of too many presents were outstanding), a present that was reported without a display time (the image did not reach the
display) and a present whose report did not arrive before its row closed. Only the stages the surface supports have values.

### The events

Event | Details
---|---
`fact` | `key=value`, something that holds for the whole run: `formatVersion`, `clock`, `clockNativeFrequency`, `utcNanoseconds` with the `utcClockTicks` it was read at (so a tick can be placed in wall clock time), `app`, `debugBuild`, `api`, `apiVersion`, the settings of the marker (`marker.*`), of the log (`log.openFrames`), the Vulkan device (`vulkan.deviceName`, `vendorId`, `deviceId`, `driverVersion`, `apiVersion`, `calibratedTimestamps`, `presentTimingDevice`, `presentTimingOption`) and the sample (`sample.presentMethod`, `sample.pacerSupported`).
`column` | The name, the unit and the description of a column.
`window` | The size and the DPI of the window, written when it changes.
`display` | `refreshIntervalTicks`: the refresh interval of the display as the window system reports it (0 if it does not know), written when it changes.
`runStarted`, `runCompleted` | A measured run of the marker: the run id, the sequence id, the name and the duration.
`swapchainCreated` | Vulkan: the extent, the format, the present mode, the image counts, the flags and the frames in flight of a swapchain. More than one means the window was resized or the swapchain was lost.
`presentTiming` | Vulkan: if the presents of the swapchain are timed, the stages and the time domain of the surface and what it can schedule.
`refreshProperties` | Vulkan: `refreshDuration` and `refreshInterval` of the swapchain and the refresh mode they stand for (`refreshMode` is `fixed`, `variable` or `unknown`), written when they change.
`variableRefresh` | What the window knows about variable refresh on its display, written when an answer changes: `supported`, `enabled` and `active` are what the window system declares, `observed` is what was measured (each `yes`, `no` or `unknown`), `source` and `observedSource` are where they came from.
`holdVariableRefresh` | The samples: `seen=1` from the frame on where the display was seen to refresh at a variable rate and the vsync wait is not used anymore.
`presentClockCalibration` | Vulkan: the offset between the clock of a present stage and the clock of the framework and how far off it can be, every time it is measured.
`pacerConfig` | The samples: the pacer was switched or its settings changed (the refresh rate it uses, the target fps, adaptive, present feedback, how a frame is held and the phase of the vsync wait).

### Adding values from an app

An app or a host adds its own columns through `IFramePacingFrameLog`. Every call does nothing while the log is off, so the only check
that is needed is the one for the service.

```C++
#include <FslDemoService/FramePacingMarker/IFramePacingFrameLog.hpp>

// In the constructor, columns can be added until the first frame is written
m_frameLog = config.DemoServiceProvider.TryGet<IFramePacingFrameLog>();
if (m_frameLog)
{
  m_columnPhysics = m_frameLog->RegisterColumn("physicsTicks", FramePacingLogUnit::DurationTicks, "How long the physics of the frame took");
  m_frameLog->SetLogFact("scene", "city");
}

// During the update or the draw of a frame: a value of the frame that is being drawn
if (m_frameLog)
{
  m_frameLog->SetLogValue(m_columnPhysics, physicsTime);
  // A value that is known later is written to the frame it belongs to
  m_frameLog->SetLogInt64At(frameIndex, m_columnResult, result);
  m_frameLog->AddLogEvent("levelLoaded", "name=city");
}
```

## Notes

- The marker must reach the capture unmodified: it is drawn opaque, pure black/white and pixel aligned at the swapchain resolution.
  Use a lossless capture and keep at least 3 stored pixels per module (`--FramePacing.CaptureHeight` computes that for you).
- HDR swapchains may alter pure black and white, prefer SDR apps for measurements.
- On OpenGL ES the `--Stats` overlay is drawn after the marker, avoid combining the two if the stats overlap the markers.
- If the marker is enabled at runtime (instead of on the command line) it is first shown the frame after it was enabled, as the render
  resources it needs are created on demand.
- The SDK is pinned to a commit of mb-framepacing until the first `sdk-v0.1.0` release is published.
- The mb-framepacing tools must be of the marker format the app draws. Since mb-framepacing `5908e9d` every marker ends with a CRC-32
  of its bytes (the markers keep their size and place): tools built before it do not decode the markers of this build, and tools built
  from it do not decode a recording of an older build.

## Implementation

Package                                    | Content
-------------------------------------------|--------------------------------------------------------------------------------------------
`ThirdParty/mb_framepacing`                | The mb-framepacing C++ SDK: its marker module and, for the samples, its experimental pacer module (via `Recipe.mb_framepacing_0_1`).
`FslDemoService.FramePacingMarker`         | The public `IFramePacingMarkerService` and `IFramePacingFrameLog` interfaces (header only, available on all platforms).
`FslDemoService.FramePacingMarker.Control` | The host side `IFramePacingMarkerServiceControl` and `IFramePacingOverlay` interfaces (header only, available on all platforms).
`FslDemoService.FramePacingMarker.Impl`    | The service, its command line options, the run state machine, the overlay that draws the marker and the frame log (the table of open rows, the CSV formatter and the writer thread).

The hosts only use the Control interfaces: they get `IFramePacingMarkerServiceControl` with `TryGet` and create the overlay through it. The
service is only registered (by `FslDemoPlatform`) on the platforms that support the marker library, everywhere else `TryGet` returns
null and nothing is drawn.

The overlay draws the library's static grids: `MB::FramePacing::Marker::GridVertices` for the main and the sync marker, only regenerated
when the window size or the marker options change. Every frame it encodes the marker with `MB::FramePacing::Marker::GenerateModules` and
uploads just the 16-bit indices from `MB::FramePacing::Marker::ModulesToGridIndices` (about 5 KB). It renders on the GPU through the FslGraphics3D
`IBasicRenderSystem` (one dynamic vertex buffer, one dynamic index buffer, an opaque material without depth test or culling and a pixel
aligned orthographic projection), so the same code is used for OpenGL ES 2, OpenGL ES 3 and Vulkan. The host draws it inside the frame after the app has drawn (`DemoAppManager` for OpenGL ES and
`DemoAppVulkanBasic::AddSystemUI` for Vulkan).
