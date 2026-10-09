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
`--FramePacing.Run <name>`        | Start a measured run with this name at the first frame. The name is written to the trace next to the run's random sequence id. Implies `--FramePacing`.
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
markers the run start time and sequence id (`FramePacingSequenceId`). Times are in nanoseconds, as the marker carries them
(`NanosecondTimeSpan`/`NanosecondTickCount`), and a value that is unknown
(the intended display time, target frame time and preferred frame time, unless the app supplies them) is an empty `std::optional`. The
marker is drawn at the end of the frame, so during a frame this is the previous frame's marker.

The framework has no frame pacer. An app with its own frame pacer calls `SetFrameSchedule(FramePacingFrameSchedule)` during its draw,
before the marker is drawn (on Vulkan before `AddSystemUI`), to supply the values it paced the frame by: the animation time, and
optionally the CPU start time, intended display time, target frame time, preferred frame time and the static after flag. It applies to that
frame only, a frame without the call reports the framework's values. The times of the schedule are in nanoseconds
(`NanosecondTimeSpan`/`NanosecondTickCount`) and go into the marker as they are given, so a frame pacer that counts in nanoseconds
loses nothing: a refresh period of 4166389 ns is that in the marker. A time read on a clock of the framework (a `TimeSpan` or a
`TickCount`) is a whole number of 100 ns ticks, which `NanosecondTimeSpanUtil::FromTimeSpan` and
`NanosecondTickCountUtil::FromTickCount` convert exactly. The values the framework reports by itself are such times.

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
SDK (`MB::FramePacing::Pacer::TierPacer`). It is only part of the samples (`SamplePacer` in
[Shared/FramePacing](../DemoApps/Shared/FramePacing)), as its API may change in any release. The recipe pins a commit of mb-framepacing.

The pacer holds every rule and every time calculation. It is told what the app can do and which of that it is to use, and it says
what a frame waits for before it starts and before it is presented. The sample only supplies information and carries out the waits it
is given, see [below](#how-the-pacer-paces-a-frame---pacerkind). The pacer can be given when the display showed the frames (display
reports, see below), which it counts and does not pace by.

The pacer needs nothing but a steady clock, a wait until a time and the refresh period of the display. Every frame the sample gives it
the time the frame starts and gets back the swap interval of the frame (the number of display refreshes it is shown for), the time step to
animate the frame by and the pacing values of the marker. The sample hands those to the marker with `SetFrameSchedule`, so with the pacer
on the marker reports the intended display time, the target frame time and the preferred frame time.

Control                       |Argument                        |Description
------------------------------|--------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Frame pacer (or the **P** key)|`--Pacer`                       |Switch the frame pacer on and off.
Refresh rate                  |`--Pacer.RefreshRate <hz>`      |The refresh rate of the display. It is read from the window system, the slider only sets it when the window system does not know it. The argument overrides both and allows decimals (59.94).
Target fps                    |`--Pacer.TargetFps <fps>`       |The frame rate the pacer aims for, 0 is the refresh rate of the display. 30 on a 60 Hz display holds every frame for two refreshes.
Adaptive swap interval        |`--Pacer.Adaptive <true\|false>`|On: the pacer slows down when frames are late and speeds up again when they fit. Off: a fixed frame rate.
Display reports to the pacer  |`--Pacer.DisplayReports <true\|false>`|Vulkan only. On: the pacer is given when the display showed the frames and counts what the display did, see [below](#display-reports-vulkan-optional). It paces the same on and off. Off is the default.
                              |`--Pacer.StartupPause <refreshes>`     |The refreshes of the pause the pacer makes once, half a second after it started, so the presents that are queued between the app and the display are shown before the next one is added (`timer-period` and `vblank-period` with the aim of low latency; 0 to 32, the default is 4, 0 is none).
Low latency (off: smoothness) |`--Pacer.Aim <smoothness\|low-latency>`|What the pacer optimizes for, see [below](#how-the-pacer-paces-a-frame---pacerkind). The default is `smoothness`.
Timed present                 |`--Pacer.TimedPresent`         |The pacer uses the present that takes a time, where the app has one (Vulkan with a swapchain that takes a time the frame before stays on screen at least): the pacer plans that time and the sample gives it to the present, next to everything it does without it. It changes no tier: only a present at a time lets the display place the frame.
                              |`--Pacer.GpuWait`              |The kinds without a wait for a present (`timer-period` and `vblank-period`) hold the frame loop with a wait for the GPU's work on an earlier frame, where the app can make that wait (Vulkan): the pacer names the frame, the one before with the aim of low latency and the one before that with smoothness where two frames are in flight (`--VkFramesInFlight 2`). It is the wait for a frame slot of the app base, made for the frame the pacer names and reported to it.
                              |`--Pacer.SystemWaits <true\|false>`|true (default): the waits the app base makes by itself before a frame (the wait for a frame slot and the acquire of a Vulkan app) are reported to the pacer. On a timer the pacer then does not take a frame the system held for a late one, and on vertical blank times a start the side of the display held is not stepped over by the animation time. false: they are not reported, for a run to hold a run with them against.
                              |`--Pacer.WaitingPresents <n>`   |The presents the pacer lets wait to be shown while a frame is made, the frame itself counted (1 to 8). It is for measuring: not given, the pacer picks the number, which is what a app leaves to it (it picks two).
Tier (radio buttons)          |`--Pacer.Kind <timer-period\|timer-present-wait\|vblank-period\|vblank-present-wait>`|What the frame loop paces with, which is the sub tier that is asked for, see [below](#how-the-pacer-paces-a-frame---pacerkind). The default is `vblank-present-wait`, the best of the four. A app that lacks what a kind uses is paced with the best kind of what it has, so the default is the best tier that is available.
                              |`--Pacer.PresentAtTime <true\|false>`|true (default): the pacer uses the present that takes a time before which the frame is not shown, where the app has one (Vulkan with a swapchain that takes a absolute target time), so the display places the frame: tier 2, and tier 1 where the display leaves out a frame that is overdue (the present mode FIFO latest ready). false: the frame loop places the frame, tier 3. A app without such a present is in tier 3 with both.
                              |`--Pacer.SystemHoldsLoop`       |Tell the pacer that the system holds the frame loop while its queue of frames is full (a acquire or a present that waits for the display), for `timer-period`. With the aim of smoothness the pacer then lets the system pace the loop, and a frame the system held is not late. Only for a system that does hold the loop. The Vulkan sample reports its wait for the frame slot and its acquire to the pacer with or without it, and the images of its swapchain.
                              |`--Pacer.ReadyPlace <percent>`  |Where in a refresh the pacer has a frame ready with `vblank-period` and `vblank-present-wait` (presented, and with the GPU work reported the GPU done with it), in percent of the refresh after a vertical blank (0 to 100, the default is 50). A frame that is ready there is shown at the next vertical blank: earlier leaves more room for a frame that runs long, later shows a newer frame.
                              |`--Pacer.KindChange <frames>`   |For measuring what a change of the kind does: every that many frames the run goes on with the next of the four kinds (`--Pacer.Kind` says the first), in a order that has every change from one of them to another once in twelve changes. The pacer is not started again by it. `0`, the default: the kind is not changed.
CPU load                      |`--CpuLoad <ms>`                |The time in milliseconds the app spends busy every frame.
                              |`--CpuSpike <ms>`               |A frame that runs long now and then: the time in milliseconds one frame in every `--CpuSpike.Interval` frames is busy on top of the CPU load (0 to 200, 0 is none and the default).
                              |`--CpuSpike.Interval <frames>`  |The number of frames from one frame that runs long to the next (2 to 100000, the default is 120).
GPU load                      |`--GpuLoad <steps>`             |Draws the background with the given load (0 is no background, the default is a low load of 16). The cost grows linearly with it. It is the number of steps of every ray, more steps reach further; for the lace every doubling of it adds a round of finer detail and the rest of it is samples per pixel; for the Mandelbrot zoom a pixel can take eight times that many iterations, and the more of them the more of the spirals is drawn.
                              |`--GLFlush`                     |OpenGL ES only. Call `glFlush` after the last command of a frame, so the GPU is asked to work on the frame at a known moment (`flushCallTicks` in the frame log) and before a swap the sample delays. Off by default: the driver then decides when, the swap at the latest. It is a flush at the end of a frame; a flush in the middle of one makes a tile based GPU store and load the frame again.
Background                    |`--Background <name>`           |The scene of the background (the radio buttons below the GPU load). `mandelbrot` (the default) is a zoom into a spiral of the Mandelbrot set (Seahorse Valley): the cheapest one, which a low end GPU can draw. `blobs` is a flight through blobs that melt into each other and `lace` is circles packed into circles: both are cheap at a low load. `flight` is a raymarched flight through a fractal lattice and `hall` a raymarched hall of columns that scrolls sideways at a constant speed, which makes a stutter easy to see: both cost a lot from the first step on.
Measure the presents          |                                |Vulkan only. Measure when the frames reach the display (`VK_EXT_present_timing`), see [below](#what-the-vulkan-sample-measures-about-its-presents). Start with `--VkPresentTiming false` to run without the extension.
Place the GPU work in time    |                                |Vulkan only. Show when the GPU worked on a frame, counted from the start of the frame (`VK_KHR_calibrated_timestamps`).

The target fps slider and the adaptive switch can only be changed while the frame pacer is on. The line below the refresh rate shows the
rate the frames are paced at: the refresh rate divided by the swap interval. The two status lines below the switches show the swap interval
and the frame time that was measured, with how many of the last frames were late. They are shown with the pacer off as well: every frame is
then held for one refresh, and the late frames are the ones of the last two seconds that took more than one refresh.

Below the switch of the frame pacer the side bar has what the frames can be paced with, as one group of radio buttons: every tier
of the pacer library (see [FramePacingPlatformSupport.md](FramePacingPlatformSupport.md)). The tiers are the library's and the sample has no definition of its
own. It has three major tiers by who places a frame on its refresh, each with a line of the library above its radio buttons, and
four sub tiers each, written as the two numbers with the best first: `1.1` to `3.4`.

- **Tier 1, the display places a frame and skips one that is overdue**: the present is given a time before which the frame is not
  shown, on a display side that shows the newest of the frames that are due (Vulkan with a absolute target time and the present
  mode FIFO latest ready). The library rates it and paces it as the same sub tier of tier 2.
- **Tier 2, the display places a frame**: the present is given such a time and every frame is shown (Vulkan with a absolute target
  time, `--Pacer.PresentAtTime`, which is on by default).
- **Tier 3, the frame loop places a frame**: the loop has to make the present at the right moment. Every app reaches its last sub
  tier.

A sub tier is what the frame loop paces with (`--Pacer.Kind`), each with its own color: vertical blank times with a wait for a
present (orange, `vblank-present-wait`), vertical blank times (purple, `vblank-period`), a timer with a wait for a present (blue,
`timer-present-wait`) and a timer (green, `timer-period`). In tier 3 that is their order, in tiers 1 and 2 the wait comes first
(`x.1` vertical blank times with a wait, `x.2` a timer with a wait, `x.3` vertical blank times, `x.4` a timer).

No tier is hidden: one the app does not have what it takes for is disabled. Of tiers 1 and 2 only one can be used on a system, as
what the display does with a frame that is overdue is a fact of it and not something a run chooses. The time the frame before
stays on screen at least changes no tier. That one is the switch `Timed present` (`--Pacer.TimedPresent`) below, where the present
of the app takes it: the pacer then plans that time and the sample gives it to the present, unless the present is given a time
before which the frame is not shown, as a present takes one of the two. The line below the radio buttons is the library's line for
the tier the run is in.

With `--Pacer.GpuWait` the two kinds without a wait for a present (`timer-period` and `vblank-period`) hold the frame loop with a
wait for the GPU's work on an earlier frame, where the app can make that wait (Vulkan). The pacer names the frame: the one before
with the aim of low latency, the one before that with smoothness where two frames are in flight (`--VkFramesInFlight 2`). The
sample waits for it with the wait for a frame slot of the app base (`DemoAppVulkanBasic::WaitForGpuWork`), for as long as the pacer
says at the most, and tells the pacer what became of the wait. It keeps the loop from getting ahead of the GPU and says nothing of
the display, so it changes no tier. After a wait that ran out the app base still waits for the frame slot before it uses it, as
Vulkan needs, and that wait is reported to the pacer as a wait of the system.

- The tier that is checked is the one the run is paced in, and the one it will be paced in while the frame pacer is off. The line
  below the group is the library's line for it.
- A tier that can be used here can be checked, so the tier can be switched while the sample runs. The options say which one a
  run starts with, and the default is the best one: `--Pacer.Kind vblank-present-wait` with `--Pacer.PresentAtTime true`. A tier is
  disabled when the app does not have what it takes (the library rates what the app can do): a time on the present needs a
  swapchain that takes a absolute target time, vertical blank times need a window system that tells when the display refreshes,
  and the wait for a present needs a app that can wait for one (Vulkan with `--VkPresentWait <n>`).
- What was asked for and can not be used right now gives way to the tier of what is left of it: a tier of the display placing the
  frame to the same kind where the frame loop places it, `vblank-present-wait` to `vblank-period` without a present to wait for
  and to `timer-present-wait` without vertical blank times, and each of them to `timer-period` without both. It comes back when
  it can be used again. So a run that asks for nothing is in the best tier the system has.
- With the frame pacer on the app base makes no wait for a present by itself: the pacer names the present a frame waits for, and
  `--VkPresentWait <n>` is the number of presents the pacer lets wait.

The tier the run is in and the best one the system reaches are in the `tier` event of the log and in the `FramePacing:` line of the
output. If the present of the app can hold a frame for two refreshes or more is no tier: the library rates it on its own, and it is
the `Display side holds` row of the frame pacing overlay.

Two overlays at the top right can be switched on and off (`Show the last marker` and `Show the frame pacing`, or start without them with
`--HideMarkerStats` and `--HidePacingStats`). The first shows every value of the last marker. The second shows what the frame pacing does:
on Wayland what is certain about explicit sync (`Explicit sync`: `not offered (not in use)` or `offered by the compositor`, if the
driver uses it can not be asked), the swap interval (and the one of the target frame rate), the average frame time of the last two seconds with the shortest and the longest
one, the late frames, the work of the last frame (the CPU time, and the GPU time if the app measures it), the average work the pacer decides
on as a share of the frame time, how long the present was delayed, how often the pacer made the swap interval longer (slower) or shorter
(faster) and when it last did, and the time its frame window spans. `Swap interval changes` counts from when the pacer was switched on
or one of its settings was changed last, and says for how long that is (`1 slower, 0 faster (counted for 12 s)`). The values only the
pacer has show `pacer off` while it is off. The controls at the right can be scrolled if the window is too low for them.

A time of the last marker is followed by its step from the marker before, the delta time (`DT +4.166`, in milliseconds): the
animation time, the intended display time and the CPU start time.

A value of the frame pacing overlay that asks to be looked at is yellow, and one that says the run does not do what it aims for is
red:

Value | Yellow | Red
--|--|--
`Frame time`, `Display interval` | The longest one is one and a half target frame times or more: a refresh was missed | The average is 5 % or more above the target frame time: the rate is not kept
`Late frames` | Any late frame | One frame in twenty or more is late
`Average work` | 80 % of the frame time or more | 100 % or more
`Swap interval` | Longer than the one of the target frame rate |
`Display late` | Frames were shown a refresh or more later than they were made for |
`Variable refresh` | Active or seen: the display follows the frames, nothing holds a frame for a refresh |
`Swapchain refresh` | Not the refresh rate of the display the window is on |

The target frame time is the one the pacer gives, and one refresh while it is off.

The chart at the bottom shows what every frame cost, as three values that are each measured from the start of the frame: how long the
CPU worked on it, how long the GPU did, and how long the frame took, which is to the end of the last work on it. They are not parts of a
sum: the CPU and the GPU each work for a time, and a frame takes until the later of the two is done, with the time the GPU waited before it
began in it. So each is drawn as a line of its own (`Show the work chart`, or start without it with
`--HideWorkChart`). The legend of the chart says the average of each over the last 240 frames (the one of the GPU over those of them it
was measured for). The GPU time of a frame is known a frame or more after the frame ended, so the chart is that far behind, and a frame
that was not measured has no GPU time. Where a app only knows how long the GPU worked and not when it was done (the Vulkan sample
without `VK_KHR_calibrated_timestamps`, a OpenGL ES driver without timestamps), the GPU is taken to have started when the CPU's work
ended. The Vulkan sample measures the GPU time with
timestamp queries. The OpenGL ES samples measure it with a time elapsed query (`GL_EXT_disjoint_timer_query`), with the pacer on or off.
None of the samples waits for the GPU to measure it. Without the extension the OpenGL ES samples have no GPU time and their chart
shows the CPU time only. The moving bar and box (the test pattern) can be switched
off as well (`Show the test pattern`, `--HideTestPattern`), and the sync marker at the bottom left can be switched on (`Draw the sync
marker`, or start with it with `--FramePacing.SyncMarker`).

Where the app is told when its frames were shown (the Vulkan sample with `VK_EXT_present_timing`, see the rows of the measured presents
below) a second chart above the work chart shows the animation error of every frame (`Show the animation error chart`, or start without
it with `--HideAnimationErrorChart`). A frame is drawn for a moment and is seen at another one. Between two frames that are shown one
after the other the animation should move as far as the time that passes on screen, and the difference is the animation error, which
is what is seen as stutter: animation error = how far the animation moved from the frame before it, less how long after that frame it
was shown. It is the measure of the mb-framepacing tools, and the chart draws as the animation error panel of their report does: a bar
for each frame from the line of no error, up when the frame was shown too soon and down when it was shown too late, a faint band for
what counts as no error (1 ms to each side) and a dashed line where a frame is a whole refresh off, which is a frame that was shown a
refresh late or early. The scale is two refreshes to each side, a bar that would be longer ends at the edge with a white mark. A frame
without a display time is not judged and neither is the frame after it: they are a gap. The display times are the ones the system
reports, which is not a measurement of the display: a capture of the marker is that.

The legend of the chart counts the stutter in three numbers. The stutter is the frames with a animation error of more than 1 ms:
`Last second` is how many there were in the last second, and `Run` how many there were since the start, by their cause as the
mb-framepacing tools tell them apart. `pacing`: the display was uneven, the frame or the one before it was not on screen for the time it
was to be (half a refresh or more off its target frame time). A frame that is shown a refresh late is one, and so is the frame after
it, which comes on time and has moved too much. `jitter` (the delta time jitter of those tools): the frames were shown as evenly as
they were to be and the animation moved by another time. The late frames are not one of the two, they are the `Late frames` row of
the frame pacing overlay.

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

- **OpenGL ES 2 and 3**: `eglSwapInterval`, so the swap of the host waits for the display. The pacer is told the longest swap
  interval the EGL config takes (it can be as short as one refresh) and says what the swap of a frame is given. A frame the swap
  can not hold for its whole swap interval is held for the rest by a time the pacer gives, which the sample waits until before the
  swap. The GPU time of a frame is measured with a time elapsed query (`GL_EXT_disjoint_timer_query`) and given to the pacer, the
  sample does not call `glFinish`. Without the extension the pacer is told the CPU time only.
- **Vulkan**: a FIFO present holds a frame for one refresh and there is no swap interval, so a frame that is shown for longer is
  held by a time the pacer gives, which the sample waits until before the host presents the frame. See
  [below](#what-a-vulkan-frame-loop-waits-on-and-why-the-pacer-gives-the-times). The GPU time of a frame is measured with timestamp
  queries and given to the pacer. The sample can measure when its frames reach the display (see the section after that) and, when
  asked to, gives those display times to the pacer as display reports.

#### How the pacer paces a frame (`--Pacer.Kind`)

The pacer of the library holds every rule and every time calculation, and the app only supplies information and carries out the
waits it is given. It is one pacer for every app (`TierPacer`): it is told what the app can do and which of that it is to use, and
it paces a frame the way the tier of that set says. `--Pacer.Kind` is which of it the pacer is to use: the sample tells the pacer
what the app has (the vertical blank times of the window system, a swapchain that can be waited on), and the kind is what of that
the pacer uses. A change from one kind to another (the radio buttons, or `--Pacer.KindChange`) is a change of that set and starts
nothing again: the frames and their ids, the animation time and the swap interval go on, and the change is in force from the frame
after the one that is open.

`--Pacer.Kind` | What the pacer uses | What the sample does
---|---|---
`timer-period` | Neither the vertical blank times nor a wait for a present: for a app with a clock, a wait until a time and the refresh period of the display, which every app has | It asks the pacer before a frame starts and when the work of the frame is done, and waits until the time it is given.
`timer-present-wait` | The wait for a present: the same for a app that can also wait until a present was shown | The same, and before a frame it waits for the present the pacer names. Vulkan with `--VkPresentWait <n>`, where `n` is then the number of presents the pacer lets wait while a frame is made, and no longer a wait of the host. Without a swapchain that can be waited on the sample uses `timer-period`.
`vblank-period` | The vertical blank times: for a app that is also told when the display refreshes | As `timer-period`, and before a frame is planned it gives the pacer the newest vertical blank of the display the window is on (what the window system reports, see [the platform support](FramePacingPlatformSupport.md)), each one once. The pacer aims a frame at a vertical blank with it: with the aim of low latency it holds the start of a frame so the frame is ready at a place in the refresh before it (`--Pacer.ReadyPlace`), with the aim of smoothness it starts at once and holds the present. Until it has a vertical blank it paces on its clock as `timer-period` does. Without a window system that tells when the display refreshes, or with variable refresh seen, the sample uses `timer-period`.
`vblank-present-wait` (the default) | Both: for a app that is told when the display refreshes and can wait until a present was shown | Both of the above: it gives the pacer the vertical blanks as for `vblank-period` and waits for the present the pacer names as for `timer-present-wait`. A wait that held the loop tells the pacer which vertical blank a frame was shown at, and it learns from that where in a refresh a frame has to be ready (it starts at `--Pacer.ReadyPlace` and moves earlier; the frame log has where it is as `pacerReadyPlaceTicks`). It makes no pause after a start, the wait empties the queue. Without the wait the sample uses `vblank-period`, without the vertical blank times `timer-present-wait`.

The pacer has two aims, and what trades smoothness against latency belongs to the aim and not to the kind
(`--Pacer.Aim`, the switch `Low latency (off: smoothness)`):

- `smoothness` (the default, as in the library): the display is kept supplied. At one refresh per frame the pacer keeps frames that
  wait to be shown as a reserve, `--Pacer.WaitingPresents` less one (one by default), so a frame that runs a little long is covered
  by the reserve and gives up no refresh. A frame reaches the screen that many refreshes later. The sample does nothing for it:
  the pacer gives the first frames of a start no time to wait until, and the frames after them a time one period earlier.
  `timer-period` makes no pause after a start with this aim. At two refreshes per frame or more there is no reserve.
- `low-latency`: the frames that wait to be shown are kept as few as the pacer can, and a frame the reserve would have covered is
  a repeated one.

The sample computes no time. A frame is these calls, in this order:

1. **The present of the frame before is reported** (`AddPresent`): when it was called, when it returned and if the system took
   it. The Vulkan sample has that from `GetLastPresentCalls` of `DemoAppVulkanBasic`. The OpenGL ES samples do not present
   their frames, the host does (`eglSwapBuffers`), so the sample asks the frame pacing service
   (`IFramePacingMarkerService::TryGetLastSwapTimes`).
2. **The pacer is asked what the start of the frame waits for** (`PlanFrame`), before the frame takes anything: on Vulkan before
   the frame waits for its frame slot and acquires a swapchain image (`OnVulkanFrameStart`), on OpenGL ES in the update, which is
   the first thing after the swap. The answer is a present to wait for (the kinds with a wait for a present) or the GPU's work
   on a frame to wait for (`--Pacer.GpuWait`), and then a time to wait until. Each can be absent.
3. **The wait for the present is made and reported** (`AddPresentWait`): the Vulkan sample calls `WaitForPresent` of
   `DemoAppVulkanBasic` with the present and the timeout the pacer gave, and tells the pacer when the wait began, when it ended
   and if it ended because the present was shown. A wait for the GPU's work is made and reported the same way (`AddGpuWait`).
   The pacer is then asked again (`PlanFrame`): the wait can have taken long, and the time to wait until can have moved with what
   the wait told the pacer about the display.
4. **The sample waits until the time**, and the frame starts (`BeginFrame`): the pacer gives its swap interval, the step to animate
   it by and the pacing values of the marker. On Vulkan the frame starts after the acquire, so the wait for the frame slot and
   the acquire are between the wait and the start: they are reported to the pacer as waits of the system (`AddSystemWait`,
   `--Pacer.SystemWaits`).
5. **The work of the GPU on a earlier frame is reported** (`AddGpuWork`), for every frame the sample has measured, frames later:
   when the GPU began and ended it where the app knows both (Vulkan with `VK_KHR_calibrated_timestamps`), when it ended and how
   long it took (OpenGL ES with a timestamp), or only how long it took.
6. **The work of the CPU is done** (`EndFrame`): the pacer says how the frame is presented. A frame of one refresh is presented
   at once. A frame of two or more is held: by the swap interval the pacer gives a present that takes one (`eglSwapInterval`),
   and where the present can not hold it that long by a time the pacer gives, which the sample waits until before the present.
   Where the present takes a time and the run uses it the plan also has that time: the one before which the frame is not shown
   (`--Pacer.PresentAtTime`), or the one the frame before stays on screen at least (`--Pacer.TimedPresent`).
7. **The display time of a earlier frame is reported** (`AddDisplayReport`), where the app measures its presents and the run gives
   the pacer display reports.

`--Pacer.StartupPause` is no wait of the sample: it is the length of the pause the pacer makes once after a start.

What the pacer does itself on a timer: the frame starts are kept on one grid of refresh
periods on the clock. A frame that starts less than half a period late keeps its place and the frame after it is on time again.
After a frame that ran long the next one waits for the step of the grid nearest to where the loop is, so the loop is back where it
was against the display. The animation time moves on by the swap interval of a frame and is not moved to catch up, and the
pacer counts the refreshes that were lost (`pacerRefreshesBehindClock` in the frame log). `--CpuSpike` makes frames that run
long, to see it.

What to know about it:

- The pacer is told the work of the CPU on a frame (from its start to the end of its CPU work) and, separately, the work of the
  GPU. It puts the two together itself: side by side where the CPU can work on a frame while the GPU is on the one before, one
  after the other where it can not. The sample adds no GPU time to anything it gives the pacer, and says how many frames its host
  lets be in the works at the same time (the frames in flight of the Vulkan host, `--VkFramesInFlight`). `pacerGpuTimeTicks` in the
  frame log is the GPU time the pacer judges with.
- `timer-period` can not see the display: a refresh the display lost by itself leaves a present waiting, at one refresh per frame
  for good. `timer-present-wait` is the kind that bounds the presents that wait.
- With the low latency aim `timer-period` makes one pause half a second after it started, so the presents that queued up while the window was new are shown
  before the next one is added. The pause is the pacer's: the sample only waits for the times it is given. `--Pacer.StartupPause` says
  how many refreshes it is (4 by default, 0 is none), and `pacerStartupPauses` in the frame log counts them.
- When the swapchain is made anew the Vulkan sample tells the pacer (`ForgetPresents`): no present of the swapchain before can be
  waited for, `timer-period` makes its pause once more, and nothing else of the pacer changes. `WaitForPresent` refuses a present of
  a swapchain from before by itself as well and returns at once.
- A wait for a present ends after a few swap intervals of the frame at the latest (four, and 50 ms at the least), which is what
  the pacer gives as its timeout. A present that is never shown does not hold the loop for longer than that.
- The CPU busy time of the marker is the pacer's as well (`CpuBusyAt`). The marker of a frame is drawn while the work of the frame
  goes on, so the sample asks the pacer as the last thing of its draw and gives the value with the schedule of the frame
  (`FramePacingFrameSchedule::CpuBusyTime`). What is left of the frame after that is the host drawing the marker. For every
  other app the frame pacing service measures it, to where the marker is drawn.
- The frame log says what the pacer paced a frame with (`pacerKind`), the `pacerConfig` event has it by name, and the waits are
  in these columns: `frameWaitStartTicks` and `frameWaitTargetTicks` for the time before the frame, `presentWait*Ticks`
  for the time before the present, `waitForPresent*` for the wait for a present.

#### What a Vulkan frame loop waits on, and why the pacer gives the times

Core Vulkan has no swap interval: a FIFO present shows a frame at the next refresh, and nothing in Vulkan says when a refresh
happens. A FIFO present was expected to make the loop wait for the display and does not: `vkAcquireNextImageKHR` and
`vkQueuePresentKHR` were measured to return at once, and the loop then only waits for the GPU to finish the frame before (the fence
of its queue submit). With light work the display still paces the loop through the swapchain image the submit waits for. With work
close to a refresh it does not: the loop runs at the speed of the GPU, a little faster than the display, and presents are dropped
(97 of 2340 at 240 Hz with GPU work of 92 % of a refresh). The numbers are in [FramePacingCapture.md](FramePacingCapture.md).

So when a frame starts and when it is presented are the pacer's to say, for every frame, also one that is shown for a single
refresh, and the sample waits for the times it is given. What the pacer has to go by is the kind:

- **A timer** (`timer-period`, `timer-present-wait`): it needs nothing. The timer does not know where the refreshes are, so where
  a present lands in a refresh is chance: when it lands near a vertical blank, a frame is shown a refresh too early and its
  neighbour a refresh too long.
- **The vertical blank times** (`vblank-period`, `vblank-present-wait`): the pacer aims a frame at a vertical blank. It needs no
  Vulkan extension, only a window system that says when the display refreshes (`INativeWindow::TryGetVSyncInfo`: Windows, Wayland
  with presentation-time, X11 with Present and Android from API level 33; `--VSyncSource` selects where the window takes the time
  from, see [FramePacingPlatformSupport.md](FramePacingPlatformSupport.md)). Only a part of a refresh is safe to have a frame ready
  in, and where that part is depends on the platform (`--Pacer.ReadyPlace`; with a wait for a present the pacer learns it). It is
  not for a display with a variable refresh rate (G-SYNC, FreeSync): the vertical blank follows the frames there. The sample
  stops giving the pacer the vertical blank times when it sees that (see below).
- **A present that is given a time before which the frame is not shown** (`--Pacer.PresentAtTime`, with every kind): the display
  places the frame, so it is shown at the refresh it is for whenever the present is made before that (`VK_EXT_present_timing`
  where the device and the surface have `presentAtAbsoluteTime`). The time is the pacer's, on the clock of the framework, and the
  app base converts it to the time domain of the swapchain. The trace has what the present was given as a time of that domain
  (`presentTargetAbsoluteNs`), and the `presentTiming` event has `canPresentAtTime=1` where a present can take such a time. It is
  built and was not run: the driver of the system the captures were made on has the relative form only.
- **A present that takes a time** (`--Pacer.TimedPresent`, with every kind): the presentation engine holds the frame as well. The
  present is given the time the frame before stays on screen at least (`VK_EXT_present_timing` where the device and the surface
  have `presentAtRelativeTime`): the image is not shown before that time has passed since the image of the present before it was
  shown, and then at the first refresh. The time is the pacer's. The frame log has what the sample asked the present for
  (`presentTargetTicks`) and what the present was given (`presentTargetRelativeNs`), and the `presentTiming` event has
  `canSchedule=1` where a present can take such a time.

Waiting alone does not shorten the way from a present to the display. In a loop that does not wait a present reached the display
15.3 ms after its call with light work at 240 Hz (3.7 refreshes). A trace of the presents
(PresentMon) shows where that time is on Windows with a NVIDIA driver, which runs the Vulkan swapchain on a DXGI swapchain: the
first dozen presents of a window go through the compositor and take three refreshes from the present of the driver to the display,
and the presents the app makes meanwhile queue up in the driver. Then the window is flipped directly, the present of the driver
reaches the display in one refresh (4.0 ms), and the app's presents still wait 5.6 to 7.2 ms in front of it: a loop that makes one
frame per refresh does not work a queue off.

The pause of the pacer is for that (`--Pacer.StartupPause <refreshes>`): half a second after it started it waits that many refreshes
more, once, so what is queued is shown. A frame that is late after it adds to the queue again, and a pause that is made once does
not take that off. The wait for a present does (`timer-present-wait`, `vblank-present-wait`), which is why those kinds make no
pause.

The vertical blank times are also left when the display was seen to refresh at a variable rate: the window says so
(`INativeWindow::TryGetVariableRefreshInfo`, on Windows a measurement of the vertical blanks) or the swapchain does. From then on
the pacer is not given them, the two tiers that use them are disabled and the run goes on with the kind of what is left, until the
refresh rate of the display changes, so the tier does not go back and forth. The `Variable refresh` row of the frame pacing overlay
shows what is known: `not seen`, `seen` with the refreshes of the mode between two refreshes of the display, `seen before`, or
`unknown`, and in the Vulkan sample what the swapchain says (`swapchain: fixed`). `not seen` is no proof that variable refresh is
off: a display with it on refreshes like a fixed one while the frames come at the rate of its mode. The frame log has the event
`variableRefreshSeen` where the sample stopped giving the pacer the vertical blank times. What a platform can tell is in
[FramePacingPlatformSupport.md](FramePacingPlatformSupport.md#variable-refresh-what-a-platform-tells-an-app).

Any Vulkan app can give a present such a time, before the frame is presented: `DemoAppVulkanBasic::IsPresentAtTimeSupported()` and
`SetPresentAbsoluteTargetTime(time)` for the time before which the frame is not shown, `IsPresentSchedulingSupported()` and
`SetPresentRelativeTargetTime(time)` for the time the frame before stays on screen at least. A present takes one of the two, the
first where both are given.

#### Display reports (Vulkan, optional)

`--Pacer.DisplayReports true` (the `Display reports to the pacer` switch) gives the pacer the time the display showed each frame,
as display reports. It is statistics only: the pacer counts the animation error of the frames from them and paces by none of it
(the swap interval, the animation step, the start of the next frame and the late frames of its frame window all come from the frame
starts and the work of the frames). It needs the presents to be measured (`VK_EXT_present_timing`, the `Measure the presents`
switch), so it does nothing for the OpenGL ES samples, and it is off by default.

A frame is judged when it and the frame before it both have a display time: its animation error is its animation time step less
the time between the two display times.

What the sample does with it:

- Every frame it remembers the id the pacer gave the frame next to the id of the present of the frame. When the display time of a
  present arrives, a few frames later, it gives the pacer the display time of that frame (`AddDisplayReport`). It tells the pacer
  nothing about a frame without one: no display time is not "never shown". A present that was not asked to be timed is not
  reported.
- The waits of the sample are the ones the pacer gives, with and without the reports.
- The `Display reports` row of the frame pacing overlay shows the frames the pacer judged and the display times it refused. The
  `Display late` row is the frames that were shown a refresh or more later than they were made for, next to the `Late frames` the
  pacer counts from the frame starts. The frame log has it per frame (`pacerDisplayReportsOn`, `pacerFrameId`, `pacerDisplayReportTicks`)
  with the pacer's counts (`pacerDisplayReports`, `pacerDisplayRefused`, `pacerDisplayJudgedFrames`, `pacerDisplayErrorFrames`,
  `pacerDisplayOffTargetFrames`, `pacerDisplayLateFrames`) and the time from the start of a frame to its display
  (`pacerDisplayStartToDisplayFrames`, `pacerDisplayStartToDisplayTotalNs`, `pacerDisplayStartToDisplayLongestNs`).
  `pacerDisplayLateFrames` is the count of the display to hold against `pacerWindowLateFrames`, the count the pacer paces by.

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
  - `Animation error`: how many of the last frames that could be judged (240 at the most) had a animation error of more than 1 ms, and
    the worst one with its sign (negative: shown too late). It is what the animation error chart draws.
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

The frame log is the trace of the app: `--Trace <file>` makes it write what every frame did, so the frame loop can be looked at
afterwards and compared with a capture of the marker. It works with every OpenGL ES and Vulkan app and does not need the marker to be
drawn. How to capture runs that can be compared, with a plan, a known load on the machine and notes, is described in
[FramePacingCapture.md](FramePacingCapture.md). What a trace is, how to open it and how to read it with a tool is described in
[Trace.md](Trace.md).

```bash
Vulkan.FramePacing --Pacer --Trace frames.perfetto-trace --ExitAfterFrame 2000
```

What this document calls the frame log is the part of the trace that is about the frames:

- **The values of a frame**: every frame the host began has a `Frame` span, and every value of the frame is an argument of it under
  its name. `frameIndex` is the frame index the marker of the frame carries. The tables below list the values, which are the
  columns where the frames are read as a table (`.Config/FramePacing/FramePacingTraceFile.py`, or the CSV files
  `FramePacingTraceToCsv.py` writes).
- **The events and the facts**: what is not a value of a frame, see [the events](#the-events).

Every value of a frame is a whole number and nothing is rounded or converted to a unit with decimals. A time (`ticks`) is a time of the
steady clock of the framework (`HighResolutionTimer`) in 100 ns ticks and a duration (`durationTicks`) is in the same ticks. A value of
the driver is written as the driver gave it, with its unit in its name (`refreshDurationNs`). A frame does not have a value when the
app does not supply it, the extension is missing, or the value did not arrive. What a frame pacer gives the marker is written in
nanoseconds, as the marker carries it (`animationTimeNs`, `intendedDisplayNs`, `targetFrameTimeNs`, `preferredFrameTimeNs`). The
exceptions to "nothing is rounded" are two times of the marker that are written in ticks, where a app gave them in nanoseconds:
`cpuStartTicks` is the tick the time lies in and `markerCpuBusyTicks` is rounded to the nearest tick.

The trace is written by a thread of its own, the render thread only hands over what it recorded. A frame stays open for 64 frames, as
the values of a frame arrive over time (the display time of a present is reported a few frames later), and is written when it closes.
A app that is killed loses the frames that were open and about a second of what came before.

The app used to write the frame log itself as two CSV files (`--FramePacing.Log`). That option is gone.
`.Config/FramePacing/FramePacingTraceToCsv.py` writes those files from a trace for a tool that reads them, see
[FramePacingCapture.md](FramePacingCapture.md#csv-files-from-a-trace).

### The columns

Which values a frame has depends on the app: every app has the ones of the service and the host, a Vulkan app adds its presents and
the FramePacing samples add their pacer. The `Schema` track of a trace describes every value of the trace it is in: its name, its unit
and what it is.

**The service, for every app**

Column | Unit | Description
---|---|---
`markerKind` | code | The kind of marker of the frame: 0 frame, 1 the start of a run, 2 the end of a run
`runId` | id | The id of the current (or last) run, as the marker carries it
`runState` | code | The state of the run: 0 idle, 1 starting, 2 measuring, 3 ending
`animationTimeNs` | nanoseconds | The time the frame is animated for in nanoseconds, as the marker carries it (the one of the app if it gave a schedule)
`cpuStartTicks` | ticks | When the CPU started on the frame, as the marker carries it to the tick (the one of the app if it gave a schedule)
`hostCpuStartTicks` | ticks | When the host started the update of the frame
`beginFrameTicks` | ticks | When the host told the service the frame begins: after the update and after the frame was prepared for drawing
`hasSchedule` | flag | 1 if the app gave the pacing values of the frame
`intendedDisplayNs` | nanosecondTicks | When the frame pacer of the app intends the frame to be shown, as the marker carries it
`targetFrameTimeNs` | nanoseconds | The frame time the frame pacer of the app aims for in nanoseconds, as the marker carries it
`preferredFrameTimeNs` | nanoseconds | The frame time the app wants to run at in nanoseconds, as the marker carries it
`markerDrawn` | flag | 1 if the marker was drawn on the frame
`markerDrawTicks` | ticks | When the marker was drawn, the last thing of the frame
`markerCpuBusyTicks` | durationTicks | How long the CPU worked on the frame before the marker was drawn, as the marker carries it to the tick
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
`displayRefreshPeriodNs` | nanoseconds | The time between two refreshes of the display as the window system measured it or has it for the mode of the display, in nanoseconds. It is read with displayVSyncTicks
`displayVSyncFlags` | code | What the window system says about how displayVSyncTicks was obtained (NativeWindowVSyncTimeFlags), zero where it says nothing: 1 the frame was shown in sync with the display, 2 a time of the display hardware, 4 the hardware signalled the frame was shown, 8 zero copy. Wayland sets them from the kind flags of presentation-time
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
`frameSlotWaitBeginTicks` | ticks | When the host began to wait for the frame slot of the frame: for the GPU to finish the frame that used the slot before, and for the present fence of that frame. It is before the acquire
`frameSlotWaitEndTicks` | ticks | When the wait for the frame slot of the frame ended
`waitForPresentBeginTicks` | ticks | When the host began to wait for an earlier present to be presented (vkWaitForPresent2KHR, `--VkPresentWait`). It is the first thing of a frame, before the app holds its start. Empty: the host did not wait
`waitForPresentEndTicks` | ticks | When the wait for an earlier present ended
`waitForPresentId` | id | The presentId of the present the host waited for
`waitForPresentResult` | code | The VkResult of vkWaitForPresent2KHR (0: the present was presented, 2 is VK_TIMEOUT: it was not within the wait)
`gpuWorkWaitBeginTicks` | ticks | When the host began to wait for the GPU to be done with an earlier frame the app named (DemoAppVulkanBasic::WaitForGpuWork). It is before the wait for the frame slot. Empty: the app asked for none
`gpuWorkWaitEndTicks` | ticks | When the wait for the GPU's work on an earlier frame ended
`gpuWorkWaitPresentId` | id | The presentId of the frame whose GPU work the host waited for
`gpuWorkWaitResult` | code | The VkResult of the wait for the GPU's work (0: the GPU is done with the frame, 2 is VK_TIMEOUT: it was not within the wait)
`acquireFenceWaitBeginTicks` | ticks | When the host began to wait for the fence of the acquire: for the swapchain image to be free (`--VkAcquireFenceWait`). It is right after the acquire. Empty: the host did not wait
`acquireFenceWaitEndTicks` | ticks | When the wait for the fence of the acquire ended
`submitCallTicks` | ticks | When vkQueueSubmit was called for the frame: the GPU is asked to work on the frame from here
`submitReturnTicks` | ticks | When vkQueueSubmit returned
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
`presentTargetRelativeNs` | nanoseconds | The target time the present was given: its image is not shown before this long after the image of the present before it was shown (empty: the present was given none)
`presentTargetAbsoluteNs` | nanoseconds | The time the present was given before which its image is not shown, as a time of the time domain of the swapchain (`presentTimeDomainId`), which is not the clock of the framework (empty: the present was given none)

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
`frameWaitTargetTicks` | ticks | The time the start of the frame was held to (empty: it was not held). The frame started at `frameStartTicks`, so the difference is how late the wait woke
`frameStartTicks` | ticks | When the sample started the frame, which is the start the frame pacer is given
`endFrameTicks` | ticks | When the work of the frame was done, which is what the frame pacer is told
`workCpuTicks` | durationTicks | How long the CPU worked on the frame
`workGpuTicks` | durationTicks | The GPU time the frame pacer was told with the frame: the one of the last frame that was measured
`workGpuFrameIndex` | id | The frame (`frameIndex`) the `workGpuTicks` of the row was measured on: an earlier frame, as the GPU time of a frame is not known when the frame ends
`gpuTimeTicks` | durationTicks | The GPU time of the frame itself, written when it was measured, a frame or more later (empty: the frame was not measured)
`gpuWorkBeginTicks` | ticks | When the GPU started on the frame, on the clock of the framework
`gpuWorkEndTicks` | ticks | When the GPU finished the frame, on the clock of the framework. Vulkan: the timestamp at the end of its commands (`VK_KHR_calibrated_timestamps`). OpenGL ES: a timestamp query after its last command, where the driver has timestamps
`flushCallTicks` | ticks | OpenGL ES with `--GLFlush`: when `glFlush` was called after the last command of the frame, the GPU is asked to work on the frame from here
`presentWaitTicks` | durationTicks | How long the frame waited before its present, for the time the frame pacer gave
`presentWaitBeginTicks` | ticks | When the sample began to wait with the finished frame before its present
`presentWaitTargetTicks` | ticks | The time the wait before the present aimed at
`presentWaitEndTicks` | ticks | When the wait before the present woke, the present follows
`cpuLoadMs` | count | The CPU load setting: the milliseconds the sample is busy per frame
`cpuSpikeMs` | count | The milliseconds the frame was busy on top of the CPU load: a frame that runs long now and then (`--CpuSpike`), zero in the frames between
`gpuLoadSteps` | count | The GPU load setting: the steps of the background (for the lace the rounds of detail and the samples per pixel come from it)
`presentTargetTicks` | durationTicks | The target time the sample asked for: the frame is not to be shown before this long after the frame before it was shown
`pacerFrameId` | id | The id the frame pacer gave the frame, its reports are given with it
`nextFrameStartTicks` | ticks | The start of the frame plus its swap interval according to the frame pacer: what the waits of the sample hold to
`pacerKind` | code | What the pacer of the library paced the frame with (`--Pacer.Kind`): 1 a timer and the refresh period, 2 a timer with a wait for a present, 3 the vertical blank times and the refresh period, 4 the vertical blank times with a wait for a present. The pacer gives the times the sample waits until
`pacerRefreshesBehindClock` | count | The refreshes that were lost and that the animation time was not moved over, counted since the pacer was made, as it was when the frame started
`pacerGpuWaitTimeouts` | count | With `--Pacer.GpuWait`: the waits for the GPU's work on a frame that ended without the GPU done with it, counted since the pacer was made
`pacerPresentWaitTimeouts` | count | `pacerKind` 2 and 4: the waits for a present that ended without the present being shown, counted since the pacer was made
`pacerPresentWaitsStopped` | flag | `pacerKind` 2 and 4: 1 while the pacer does not wait for presents because its waits ran out (a window that is not shown): it paces on its timer and only asks if a present was shown
`pacerGpuTimeTicks` | durationTicks | The GPU time the pacer judges a frame with, as it was when the frame started: the newest it was given (the sample gives it the GPU work of every frame it has measured, with the times where it has them)
`pacerStartupPauses` | count | `pacerKind` 1 and 3: the pauses the pacer made after a start (half a second after its first frame, and again after a new swapchain), counted since the pacer was made
`pacerVBlankReading` | flag | `pacerKind` 3 and 4: 1 once the pacer was given a vertical blank of the display it is on. Before that it paces on its clock
`pacerVBlankJumps` | count | `pacerKind` 3 and 4: the vertical blanks the pacer was given that were more than a eighth of a refresh off where the ones before put them, counted since the pacer was made
`pacerShownLaterByWaits` | count | `pacerKind` 4: the refreshes a wait for a present said a frame was shown later than the pacer had worked out, counted since the pacer was made
`pacerReadyPlaceTicks` | durationTicks | `pacerKind` 4: where in a refresh the pacer has a frame ready now, as the time after the vertical blank before it. It starts at `--Pacer.ReadyPlace` and the pacer moves it earlier when frames that were ready there are shown a vertical blank late
`pacerReadyPlaceTries` | count | `pacerKind` 4: the times the pacer tried that place one step later again, counted since the pacer was made
`pacerReadyPlaceTriesTakenBack` | count | `pacerKind` 4: the tries it took back, because a frame was shown later in the frames after one
`pacerSystemHeldFrames` | count | `pacerKind` 1 and 2: the frames whose start the side of the display held (a acquire, a present that waited: for a eighth of a refresh or more), counted since the pacer was made
`pacerFrameSlotHeldFrames` | count | `pacerKind` 1 and 2: the frames whose start a wait for a frame slot held for a eighth of a refresh or more (the GPU was not done with a earlier frame), counted since the pacer was made
`pacerDisplayHeldRefreshes` | count | `pacerKind` 3 and 4: the refreshes frame starts were late by while the side of the display held the loop (a acquire, a present that waited, a wait for a frame slot or for the GPU's work while the GPU did no work), which the animation time does not step over, counted since the pacer was made
`pacerDisplayReportsOn` | flag | 1 if the frame pacer is given the display times of the frames: it counts what the display did, it paces the same
`pacerDisplayReportTicks` | ticks | The display time of the frame the frame pacer was given as a display report
`pacerDisplayReports` | count | With `pacerDisplayReportsOn`: the display times the pacer took as display reports, counted since the pacer was made. It counts the animation error from them and paces by none of it
`pacerDisplayRefused` | count | The display times the pacer did not take: for a frame it does not keep (more than 64 frames old) or not newer than the one before
`pacerDisplayJudgedFrames` | count | The frames the pacer judged from the display reports: the frame and the frame before it both have a display time
`pacerDisplayErrorFrames` | count | The judged frames with a animation error of more than a millisecond: their animation time step less the time between the two display times
`pacerDisplayOffTargetFrames` | count | The judged frames shown half a refresh or more off where their animation time step put them: at another refresh
`pacerDisplayStartToDisplayFrames` | count | The frames whose time from their start to their display it counted, since the pacer was made
`pacerDisplayStartToDisplayTotalNs` | nanoseconds | The sum of those times from a frame's start to its display
`pacerDisplayStartToDisplayLongestNs` | nanoseconds | The longest of those times
`pacerDisplayLateFrames` | count | Of those, the frames shown later: the frame before them was on screen a refresh or more longer than it was made for
`animationErrorTicks` | durationTicks | The animation error of the frame by the display times the system reports: how far the animation moved from the frame presented before it, less how long after that frame it was shown. Negative: shown too late. Empty unless both frames have a display time

`presentTimingRequested`, `resultReadAtFrame` and the stage times tell three cases apart: a present that was not asked to be timed
(the results of too many presents were outstanding), a present that was reported without a display time (the image did not reach the
display) and a present whose report did not arrive before its row closed. Only the stages the surface supports have values.

### Which frame and which moment a value belongs to

Most values of a row are those of its own frame, also the ones that arrive later. These are not, by design:

- `workGpuTicks` is the GPU time of an earlier frame, the one `workGpuFrameIndex` names: the pacer is told about a frame when it
  ends, and the GPU has not worked on it then. The GPU time of the frame itself is `gpuTimeTicks`. Vulkan: the frame that used the
  frame slot before, so as many frames back as there are frames in flight. OpenGL ES: the newest query that had finished, one to
  three frames back, and a frame is not measured while all four queries are in use.
- `pacerWindow*` is the frame window as it was after the frame before was measured, and `pacerDisplay*` are counters.
- `displayVSyncTicks` is a recent vertical blank, read when the frame began. `refreshDurationNs` and `refreshIntervalNs` are what the
  swapchain reported last. The load of the machine is read once a second.

The samples start a frame at a different place per API, so `frameStartTicks`, the CPU start time of the marker and what is counted
from them do not cover the same:

&nbsp; | OpenGL ES 2 and 3 | Vulkan
---|---|---
The frame starts | In the update, right after the swap of the frame before returned | In the draw, after the update, the wait for the frame slot and the acquire
`workCpuTicks` and the CPU busy of the marker cover | The rest of the update and the draw | The draw. The update of the frame is in no frame's work
Where they end | Before the swap | CPU busy where the marker is drawn, `workCpuTicks` after the submit. Both end before the wait before the present and the present: `presentCallTicks` is when the present was called
The GPU time | The elapsed time of a query: a duration without a start. Where the driver has the timestamps of the extension `gpuWorkEndTicks` is when the GPU finished the frame | The time between two timestamps. `gpuWorkBeginTicks` and `gpuWorkEndTicks` place it on the clock of the framework (`VK_KHR_calibrated_timestamps`)
When the GPU is asked to work | `flushCallTicks` with `--GLFlush`. Without it not known: the driver sends the commands when it wants and at the swap at the latest (`hostSwapCallTicks`) | `submitCallTicks`

So a log has these on the clock of the framework for every frame: the start and the end of the CPU work (`frameStartTicks`,
`endFrameTicks`), when the GPU was asked to work (`submitCallTicks`, `flushCallTicks`), how long the GPU worked (`gpuTimeTicks`)
and when it finished (`gpuWorkEndTicks`). Every wait of the sample is there with when it began, the time it aimed at and when
it woke: the one before the start of a frame (`frameWaitStartTicks`, `frameWaitTargetTicks`, `frameStartTicks`) and the one
before the present (`presentWaitBeginTicks`, `presentWaitTargetTicks`, `presentWaitEndTicks`). A Vulkan app also has the wait of
the host for the frame slot (`frameSlotWaitBeginTicks`, `frameSlotWaitEndTicks`), which is where its loop waits for the GPU, and
the two waits that are off unless asked for: for an earlier present to be presented (`waitForPresentBeginTicks`,
`waitForPresentEndTicks`) and for the swapchain image to be free (`acquireFenceWaitBeginTicks`, `acquireFenceWaitEndTicks`), see
[FramePacingPlatformSupport.md](FramePacingPlatformSupport.md#keeping-the-frame-loop-from-getting-ahead-of-the-display). Of the
values at the start of this paragraph, `gpuWorkEndTicks` needs `VK_KHR_calibrated_timestamps` on Vulkan and a driver with
timestamps on OpenGL ES (`GL_EXT_disjoint_timer_query` with bits for its timestamp counter): the app log says when it is
missing and the column is empty then. The time of the GPU is on its own clock, which is related to the clock of the framework by
reading both now and then.

On OpenGL ES the driver decides where the loop waits: in the swap, or in a GL command of the frame. A wait in a GL command is
inside `workCpuTicks` and the CPU busy of the marker, and the pacer takes it for work. `hostSwapReturnTicks - hostSwapCallTicks`
says if the swap waited.

A log has the fact `sample.frameStartRow=own` when the values the sample logs at the start of a frame are in the row of that frame.
A log of a OpenGL ES sample without the fact has them in the row of the frame before (`pacerFrameId - frameIndex` is 2 there, and 1
in a log that is right): `frameWaitStartTicks`, `frameStartTicks`, `pacerOn`, `swapInterval`, `preferredSwapInterval`,
`pacerChange`, `animationStepTicks`, `pacerWindow*`, `pacerFrameId`, `nextFrameStartTicks`, `pacerDisplayReportsOn` and its counters,
`cpuLoadMs` and `gpuLoadSteps`.

### The events

Event | Details
---|---
A fact (on the `Facts` track, not a event) | Something that holds for the whole run, as a key and a value: the trace itself (`trace.formatVersion`, `trace.clock`, `trace.utcNanoseconds` with `trace.utcClockTicks`, `trace.openFrames`, `trace.anonymised`), `clock`, `clockNativeFrequency`, `utcNanoseconds` with the `utcClockTicks` it was read at (so a tick can be placed in wall clock time), `app`, `debugBuild`, `api`, `apiVersion`, the settings of the marker (`marker.*`), the Vulkan device (`vulkan.deviceName`, `vendorId`, `deviceId`, `driverVersion`, `apiVersion`, `calibratedTimestamps`, `presentTimingDevice`, `presentTimingOption`, `presentWaitOption`, `acquireFenceWaitOption`) and the sample (`sample.presentMethod`, `sample.pacerSupported`, `sample.frameStartRow`, `sample.glFlush`).
`window` | The size and the DPI of the window, written when it changes.
`display` | The refresh interval of the display as the window system reports it for the mode of the display (0 if it does not know), written when it changes: `refreshIntervalNs` in nanoseconds and `refreshIntervalTicks` rounded to the nearest tick. It is the nominal value of the mode, the display does not refresh at exactly that rate on the clock of the system.
`runStarted`, `runCompleted` | A measured run of the marker: the run id, the sequence id, the name and the duration.
`swapchainCreated` | Vulkan: the extent, the format, the present mode, the image counts, the flags and the frames in flight of a swapchain, and the two waits it runs with (`presentWait`: the presents back the host waits for, 0 if it does not; `acquireFenceWait`). More than one means the window was resized or the swapchain was lost.
`presentTiming` | Vulkan: if the presents of the swapchain are timed, the stages and the time domain of the surface and what it can schedule.
`refreshProperties` | Vulkan: `refreshDuration` and `refreshInterval` of the swapchain and the refresh mode they stand for (`refreshMode` is `fixed`, `variable` or `unknown`), written when they change.
`variableRefresh` | What the window knows about variable refresh on its display, written when an answer changes: `supported`, `enabled` and `active` are what the window system declares, `observed` is what was measured (each `yes`, `no` or `unknown`), `source` and `observedSource` are where they came from.
`variableRefreshSeen` | The samples: `seen=1` from the frame on where the display was seen to refresh at a variable rate and the pacer is not given the vertical blank times anymore.
`presentClockCalibration` | Vulkan: the offset between the clock of a present stage and the clock of the framework and how far off it can be, every time it is measured.
`gpuClockCalibration` | The samples: the clock of the GPU was related to the clock of the framework anew (`gpuWorkBeginTicks` and `gpuWorkEndTicks` are converted with it). `readTicks` is how long the read took and `maxDeviationTicks` how far off a time can be: half the read on OpenGL ES, what the device reports on Vulkan. Vulkan, read four times a second: `clockRateDeviationPpm` is how much longer (positive) or shorter a count of the device clock was measured to take than the device states, from two reads at least two seconds apart; it is left out until it was measured, and the stated period is used until then.
`windowFocus` | The samples: the window got (`focused=1`) or lost (`focused=0`) the input focus of the window system (Windows, X11 and Wayland report it). The app keeps drawing without it. Only a change is logged, so a run that had the focus from its start to its end has none.
`background` | The samples: the scene of the background, at the first frame and when it changes. The same `gpuLoadSteps` is another amount of GPU work with another scene, so two logs are only comparable when they name the same one.
`tier` | The samples: the tier of the pacer library the run is in, at the first frames and when it changes. `tier` is the tier in use as the pacer library writes it, its major tier and its sub tier (`1.1` is the best, `3.4` the one every app reaches, `0` with the pacer off) with `tierName`, `best` the best one this system reaches with `bestName`, and `displaySideHolds` is 1 where the present of the app can hold a frame for two refreshes or more.
`pacerConfig` | The samples: the pacer was switched or its settings changed (the refresh rate it uses, the target fps, adaptive, the display reports, and `kind`: what the pacer paces with, with its `aim` and `waitingPresents`, `framesInFlight`, the `startupPauseRefreshes` of its pause, and if it uses the time on the present before which a frame is not shown (`presentAtTime`), the present that takes the time the frame before stays on screen (`timedPresent`), the wait for the GPU's work (`gpuWait`) and the waits of the system (`systemWaits`)). `refreshPeriodNs` is the refresh period the pacer was given, in nanoseconds: the one of the window system as it is, or the period of the rate of the command line or the slider. `refreshRateHz` is the same as a rate. The fact `sample.pacerKind` is the pacer that was asked for.
`pacerForgetPresents` | The samples: the pacer was told that the presents made so far are gone, because the swapchain was made anew (`reason=swapchain`).

### Adding values from an app

An app or a host adds its own values, events and facts through `ITraceService`, see
[recording from an app](Trace.md#recording-from-an-app): `RegisterValue` once, then `SetValue` in every frame, or `SetValueAt` for a
value of a earlier frame that is only known now.

A frame begins in the trace at the moment the host begins its draw. The update of a frame runs before that, so a value that is set
during the update is written to the frame before: keep it and set it during the draw. The FramePacing samples do so for what a
frame started with, as their OpenGL ES frame starts in the update.

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
`FslDemoService.FramePacingMarker`         | The public `IFramePacingMarkerService` interface (header only, available on all platforms).
`FslDemoService.FramePacingMarker.Control` | The host side `IFramePacingMarkerServiceControl` and `IFramePacingOverlay` interfaces (header only, available on all platforms).
`FslDemoService.FramePacingMarker.Impl`    | The service, its command line options, the run state machine and the overlay that draws the marker. It begins the frames of the trace and records what the marker of a frame carries.

The hosts only use the Control interfaces: they get `IFramePacingMarkerServiceControl` with `TryGet` and create the overlay through it. The
service is only registered (by `FslDemoPlatform`) on the platforms that support the marker library, everywhere else `TryGet` returns
null and nothing is drawn.

The overlay draws the library's static grids: `MB::FramePacing::Marker::GridVertices` for the main and the sync marker, only regenerated
when the window size or the marker options change. Every frame it encodes the marker with `MB::FramePacing::Marker::GenerateModules` and
uploads just the 16-bit indices from `MB::FramePacing::Marker::ModulesToGridIndices` (about 5 KB). It renders on the GPU through the FslGraphics3D
`IBasicRenderSystem` (one dynamic vertex buffer, one dynamic index buffer, an opaque material without depth test or culling and a pixel
aligned orthographic projection), so the same code is used for OpenGL ES 2, OpenGL ES 3 and Vulkan. The host draws it inside the frame after the app has drawn (`DemoAppManager` for OpenGL ES and
`DemoAppVulkanBasic::AddSystemUI` for Vulkan).
