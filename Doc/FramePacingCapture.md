# Capturing frame pacing logs

How to record what an app did with its frames, in a way that can be repeated: the same runs, the same load, and notes that say what the
machine was doing. The log itself is written by the app as a trace (`--Trace <file>`, see [Trace.md](Trace.md)), the capture tool runs
the app once for every run of a plan and checks each trace.

The tool is in `.Config/FramePacing`:

File                        | What it is
----------------------------|----------------------------------------------------------------------------------------------------------
`FramePacingCapture.py`     | The capture tool. After `prepare` it is `FramePacingCapture` on Windows and `FramePacingCapture.sh` elsewhere.
`CpuLoad.py`                | The external CPU load of a loaded run. It can be used on its own.
`WindowOnTop.py`            | Keeps the window of the app above the other windows during a run (Windows).
`FramePacingRunCheck.py`    | The checks of a log.
`FramePacingLogFile.py`     | The log of a run as the tools read it (`OpenLog`), from a trace or from the CSV files of a older capture. For scripts of your own.
`FramePacingTraceFile.py`   | Reads the trace of a run.
`FramePacingTraceToCsv.py`  | Writes the CSV files of a frame pacing log from a trace, for a tool that reads those.
`Plans/*.toml`              | The plans that come with the tool.

It needs Python 3.11 or newer. A trace is read with the trace processor of Perfetto, which the `perfetto` package brings
(`pip install perfetto`, in a virtual environment where the system does not let pip install). The package downloads the trace processor
the first time it is used. Everything else is the standard library, and a capture of CSV files is checked without the package.

## Before a capture

A frame pacing log is only worth something if it is known what the machine and the display were doing. The tool records what it can
read, the rest is up to the person at the machine.

- **Build the app as a release build.** The tool runs the newest release build of the plan's app under `FSL_GRAPHICS_SDK/build`
  (`Vulkan.FramePacing` for the plans that come with the tool), or the one given with `--exe`.
- **Set the refresh rate of the display** to the one the plan is for. The window system's rate is written to the log and a run is checked
  against the rate of the plan (or `--refresh-hz`).
- **Decide on variable refresh (G-SYNC, FreeSync)** and say so with `--fact`. With it on the display follows the frames, the
  swapchain still reports a fixed refresh mode, and the display times of a run look nothing like the ones with it off. The frame
  pacer needs it off. On Windows the window measures it and the tool reports it (`vrr: seen for n frames` in the summary, a line in
  the notes, and a warning in a plan for a fixed refresh rate), but only while the frames come slower than the rate of the display:
  a run at the display rate looks the same with it on and off. The setting of the driver does not settle it either, with G-SYNC on
  for "full screen only" a window was shown with variable refresh in some runs and not in others. So read the state, do not assume it.
- **Turn on the frame rate counter of the display** if it has one (the OSD of the monitor, not an overlay of the GPU driver). It is the
  only independent view of what the display did. Note what it showed with `--fact` or in the notes afterwards.
- **Stop everything else that works on the machine**: builds, other tools, video calls, a browser with a video, and the services
  nobody thinks of (a backup that is running, a virus scan, the indexer). A build in the background was enough to make the frame
  starts of an otherwise perfect 240 Hz run uneven. Every run records the load of the machine and a run that was meant to be idle
  gets a warning if other programs used more than 10 % of the CPUs. Presenting the frames costs 2 to 5 % by itself (the compositor
  and the kernel), which is counted as other programs.
- **Note the power plan.** It decides how fast a CPU that was idle is running again. The tool writes the active one to the notes
  (`powercfg /getactivescheme` on Windows, the cpufreq governor on Linux), so capture with the plan the app will be used with, and
  repeat a capture with the default plan if the machine runs a high performance one.
- **Know the rate of every display, not only the one the window is on.** A second display at another rate changes what the swapchain
  reports and what a present that is given a time does (see the findings below), so a capture for one rate is made with one display, or with
  all displays at that rate. Write the displays into the notes with `--fact`.
- **Leave the keyboard and the mouse alone during the runs.** The window of the app takes the keyboard focus when it opens and the
  sample reacts to keys: **P** toggles the pacer. A run where the pacer was switched gets a warning. Do not move or resize the window, a
  run with more than one swapchain gets a warning as well.
- **The window has to be seen.** A window behind another one is not shown, and its presents say nothing about frame pacing. Windows
  does not let an app that a tool started take the foreground while somebody works in another window, so on Windows the tool makes the
  window of the app topmost when it opens (above the other windows, without giving it the keyboard focus). The notes of the run say
  `window: kept on top of the other windows by the tool`, or that the tool found no window. Elsewhere, look at the first run.
- **Fullscreen is `--Window []`.** Without a `--Window` argument the window comes from the environment variable
  `FSLDEMOAPP_PREFERRED_WINDOW_RESOLUTION` if it is set, and the run is a window that only looks like fullscreen. The plans always give
  the window.

## Running a plan

```bash
# The plans that come with the tool, and the runs of one of them
FramePacingCapture list
FramePacingCapture list work-matrix-240hz

# Show what would be run
FramePacingCapture run work-matrix-240hz -o D:/captures/240hz --dry-run

# Capture. It waits for Enter, then runs one run at a time
FramePacingCapture run work-matrix-240hz -o D:/captures/240hz --fact gsync=off --fact "refresh rate set by=hand, in the OSD"
```

Argument                        | Description
--------------------------------|--------------------------------------------------------------------------------------------------------
`-o, --output <dir>`            | Where the logs and notes go (default `FramePacingCapture/<date>_<plan>` in the current directory).
`--fact <key>=<value>`          | Something only you know, written to the notes of every run. Can be given more than once.
`--set <name>=<value>`          | Set a variable of the plan, for example the GPU load settings of this machine. Can be given more than once.
`--only <run>,<run>`            | Only these runs. The name of a loaded run selects its idle and loaded pair.
`--refresh-hz <hz>`             | The refresh rate the display is set to, for a plan that is used at more than one rate.
`--frames <count>`              | The number of frames of every run, instead of the ones of the plan.
`--pause <seconds>`             | The time to wait before every run, instead of the one of the plan.
`--load-processes <count>`      | The busy processes of the CPU load, instead of the ones of the plan (0 is one for every logical CPU).
`--load-duty <share>`           | The share of the time the CPU load is busy, instead of the one of the plan (0.05 to 1.0).
`--external-load`               | The machine is under a load you started yourself (a build, for example): every loaded run is run once, without its idle twin and without the CPU load of the tool. Say what the load is with `--fact`.
`--exe <file>`, `--cwd <dir>`   | The app to run and the directory to run it in (the one that holds its `Content`).
`--overwrite`                   | Replace the logs of runs the output directory holds already. Without it the tool stops.
`--dry-run`                     | Show the runs and stop.
`-y, --yes`                     | Start without waiting for Enter.

The exit code is 0 when every run passed its checks, 1 when there were warnings and 2 when the capture could not be done.

## The plans

Plan                 | Display                         | Runs
---------------------|---------------------------------|---------------------------------------------------------------------------------
`work-matrix-240hz`  | 240 Hz, variable refresh off    | GPU work of 20, 90 and 130 % of a refresh; pacer on, pacer off and basic Vulkan; each idle and under CPU load (18 runs).
`work-matrix-120hz`  | 120 Hz, variable refresh off    | The same with CPU work of 2, 7 and 11 ms.
`work-matrix-60hz`   | 60 Hz, variable refresh off     | The same with CPU work of 3, 15 and 22 ms.
`display-reports-240hz` | 240 Hz, variable refresh off | The pacer without and with display reports (`--Pacer.DisplayReports`), GPU work of 20, 90 and 130 %, each idle and under CPU load (12 runs).
`display-reports-120hz` | 120 Hz, variable refresh off | The same with CPU work of 2, 7 and 11 ms.
`plain-vulkan-240hz`, `-120hz`, `-60hz`, `-50hz` | that rate, variable refresh off | Does the pacer work on plain Vulkan: the frames paced on a timer and on the vertical blank times (`--Pacer.Kind timer-period` and `vblank-period`) at fixed frame rates and under work of 130 %, one and two frames in flight, and the key rows with present timing off as well (22 to 30 runs).
`timed-present` | any fixed rate below 240 Hz, given with `--refresh-hz` and `--set` (see the plan) | A frame held by a present that takes a time (`--Pacer.TimedPresent`) and by a wait before the present: half the refresh rate, a slower fixed rate and CPU work of 130 %, display reports on in every run, each idle and under CPU load (12 runs).
`ready-place-sweep` | any fixed rate, given with `--refresh-hz` and `--set half_fps` | Where in a refresh a frame has to be ready: frames held for two refreshes and paced on the vertical blank times, with the place from 5 to 95 % of the refresh (`--Pacer.ReadyPlace`), each idle and under CPU load (20 runs).
`timed-present-240hz` | 240 Hz, variable refresh off | A frame held for more than one refresh by a wait before the present and by a present that takes a time (`--Pacer.TimedPresent`): a fixed 60 and 120 fps and work of 130 %, each idle and under CPU load (12 runs).
`present-options-240hz` | 240 Hz, variable refresh off | The swapchain setup at work of 90 %: one against two frames in flight (`--VkFramesInFlight`) and FIFO against FIFO latest ready (`--VkPresentMode`), pacer off and on (8 runs).
`frame-timeline-240hz` | 240 Hz, variable refresh off | Every stage of a frame in the log (the waits, the submit, the GPU begin and end, the display time) for the charts of a frame on a timeline: GPU work near a refresh paced on the vertical blank times in full screen, 60 fps paced on a timer, half the rate with a present that takes a time, work of 90 % with the adaptive pacer and work of 94 % at a fixed swap interval of one, and two swapchain images and two frames in flight.
`fixed-rates`        | Any fixed rate, one at a time   | Trivial work: pacer off, pacer on, pacer at half the refresh rate. Give `--refresh-hz` and `--set half_fps=`.
`variable-refresh`   | Highest rate, variable refresh on | Fixed frame rates of 120, 80, 60 and 30 fps, pacer off, GPU and CPU load, windowed and fullscreen.

The work matrix asks what the pacer does with a frame that takes a fifth of a refresh, one that nearly fills it and one that does not
fit: it should hold one refresh at 20 %, go to two and stay there at 130 %, and 90 % is the case where a busy machine decides. `basic
Vulkan` is the pacer on a swapchain without `VK_EXT_present_timing` and without present fences
(`--VkPresentTiming false --VkSwapchainMaintenance1 false`), which is what most devices have.

The sample sets its CPU load in whole milliseconds, which is too coarse for the 4.17 ms of a 240 Hz refresh, so that plan uses the GPU
load. How long the GPU works for a setting depends on the GPU and on the size of the window, so find the settings of the machine first:

```bash
# The numbers are examples
# It measures the scene the plans name (flight), --background selects another one
FramePacingCapture calibrate --window [0,0,1600,900]
#   --GpuLoad 256: the GPU works 1.412 ms on a frame
#   ...
#   20 % of a refresh (0.833 ms): --GpuLoad 148
# For a plan with these variables: --set gpu20=148 --set gpu90=671 --set gpu130=969
FramePacingCapture run work-matrix-240hz -o D:/captures/240hz --set gpu20=148 --set gpu90=671 --set gpu130=969
```

### Idle and loaded pairs

A run a plan marks with `loaded = true` is run twice: `<name>_idle` on the machine as it is, and right after it `<name>_loaded` with
`CpuLoad.py` running. The two differ in the load only, so what the load changed can be read from the pair. The load is a number of
processes that are busy a share of the time (the `[load]` table of the plan, by default one for every logical CPU and always busy, like
a build that uses the whole machine). It is started two seconds before the app and stopped when the app exits.

A CPU load is not every load. A build also loads the disk and the memory, and it was a build that moved the frame starts of the
sample. For a load like that start it yourself and run the plan with `--external-load`: the loaded runs are then run once, as they
are, and are checked as loaded.

```bash
# The load on its own: four processes that are busy half the time, for a minute
python .Config/FramePacing/CpuLoad.py --processes 4 --duty 0.5 --duration 60
```

### Writing a plan

A plan is a TOML file. Give its path instead of a name.

```toml
description = "What the plan is for"
app = "Vulkan.FramePacing"          # The app to run
frames = 2000                       # The frames of a run, a run can have its own
expected_refresh_hz = 240.0         # Optional: the runs are checked against it
pause_seconds = 3                   # Optional: the wait before every run
timeout_seconds = 300               # Optional: a run that takes longer is stopped
args = ["--Window", "[0,0,1600,900]"]   # Given to every run

[variables]                         # {name} in args and what, set with --set
gpu90 = "680"

[load]                              # The CPU load of the loaded runs
processes = 0                       # 0 = one for every logical CPU
duty = 1.0

[[run]]
name = "w90_pacer_on"               # The name of the files of the run
what = "GPU work of 90 % of a refresh, pacer on"
args = ["--Pacer", "--GpuLoad", "{gpu90}"]
loaded = true                       # Optional: run it idle, then under load
```

The tool adds `--Trace <output>/<name>.perfetto-trace --ExitAfterFrame <frames> -v` to every run.

## What a capture leaves behind

File                   | Content
-----------------------|------------------------------------------------------------------------------------------------------------
`<run>.perfetto-trace` | The trace of the run: every value of every frame, the events, the facts and what the main thread did in each frame. It is what the tool checks, and it opens in [ui.perfetto.dev](https://ui.perfetto.dev). See [Trace.md](Trace.md).
`<run>.run.txt`        | The notes of the run: what it was, the command line, when it ran, the load, what you told with `--fact`, the machine (OS, CPU, power plan, display mode, GPU and driver, swapchain, pacer settings) and the checks.
`<run>.app.log`        | What the app printed.
`summary.txt`          | One line per run, with its warnings.

Keep the directory together, the notes are what makes a log usable later.

A capture does not name the hardware it was made on. The trace is anonymised by the app that writes it (`--Trace.Anonymise`, which
is on unless it is turned off). What the app prints names the model of the graphics device, and the tool replaces it there with
the vendor (`NVIDIA GPU`, or `GPU` where the app does not say whose it is), sets the device id to zero and leaves the model of the
CPU out of the notes. It finds the model by the lines that report it (`- deviceName:`, `Renderer:` and `GL renderer:`). The vendor
and the driver version stay, they are what a reader of a log needs. The directories of the machine are replaced as well, in what
the app printed and in the command line of the notes: the output directory by `<output>`, the SDK by `<sdk>` and the home directory
of the user by `<home>`. `--hardware-names` keeps the models and the directories, and runs the app with `--Trace.Anonymise off`.

### Reading the summary

One line per run, for example:

```
w90_pacer_on_idle | 2400 frames | 240.02 Hz | pacer on, swap interval 1: 2400 | start step 4.167 ms (4.102 to 4.231) | shown for refreshes 1: 2330, 2: 3 | work cpu 0.09 ms gpu 3.75 ms | other programs 2 % cpu | ok
```

Part                           | Meaning
-------------------------------|---------------------------------------------------------------------------------------------------
`240.02 Hz`                    | The refresh rate the window system reported during the run.
`pacer on, swap interval 1: n` | The frames by the swap interval the pacer held them for. `pacer SWITCHED` is a run to throw away.
`start step`                   | The time from the start of a frame to the start of the next: the median, and the values 1 % of the frames are under and over.
`shown for refreshes`          | The frames by the number of refreshes from their display time to the next display time. `1: 2330, 2: 3` is three frames that stayed on the display for two refreshes. Needs `VK_EXT_present_timing`.
`vrr: seen for n frames`       | The frames during which the window said the display refreshed at a variable rate (Windows measures it). Not there when it was not seen, which is no proof that variable refresh was off.
`presents not shown or not timed` | Presents the presentation engine reported on without a display time: their image did not reach the display.
`display reports taken: n, refused: n` | With display reports: what the pacer counted from the display times it was given: the ones it took and refused, the frames it judged (`judged`), the ones with a animation error (`errorFrames`), shown at another refresh (`offTarget`) and shown later (`late`). Many refused is a warning.
`work`                         | The median CPU and GPU time of a frame.
`other programs`               | The share of the CPUs that was busy with something other than the app during the run.
`ok` or `n WARNINGS`           | The warnings are listed below the line and in the notes.

The first 60 frames of a run are left out of the times, the swapchain, the pacer and the GPU timer are still settling there. The numbers
are a first look that tells if a run is usable and roughly what it showed. They are made from the log, which holds every value.

Logs that exist already can be checked again, the traces of a capture or the CSV files of a older one:

```bash
FramePacingCapture check D:/captures/240hz/w90_pacer_on_idle.perfetto-trace
FramePacingCapture check --write-notes --refresh-hz 240 D:/captures/240hz/*.perfetto-trace
FramePacingCapture check D:/captures/older/w90_pacer_on_idle.csv
```

### CSV files from a trace

A tool that reads the CSV files of a frame pacing log can be given them from a trace:

```bash
# Writes w90_pacer_on_idle.csv and w90_pacer_on_idle.events.csv next to the trace
python3 .Config/FramePacing/FramePacingTraceToCsv.py D:/captures/240hz/w90_pacer_on_idle.perfetto-trace
# Or to a file of your choice
python3 .Config/FramePacing/FramePacingTraceToCsv.py run.perfetto-trace -o D:/work/run.csv
```

The files are those an app wrote itself as its frame pacing log (`--FramePacing.Log`) before the trace became its log, format
version 1. The frames file is what the app wrote, apart from a value above 2^63: the trace holds the 64 bits of a value and not if
it was unsigned, so such a value is written as a negative number. The events file has the same events, columns and facts, and the
facts of the trace as well (`trace.formatVersion` and the other `trace.` facts).

### The warnings

Warning                                              | What to do
-----------------------------------------------------|-----------------------------------------------------------------------------
The app exited with a code, or logged fewer frames   | Look at `<run>.app.log`.
More than one swapchain was created                  | The window was resized, moved or lost. Run it again.
The pacer was switched during the run                | A key reached the window. Run it again.
The display runs at another rate than the plan is for| Set the display mode, or give the rate with `--refresh-hz`.
The refresh rate changed during the run              | The window was moved to another display, or the mode was changed. Run it again.
Only some presents have a display time               | The presents that were reported without one did not reach the display. Compare with the idle twin: under load it is a finding, not a fault of the run.
The swapchain reports a longer refresh than the window system | Variable refresh is on, or something else has the display (a game, for example). Set it right and run it again. A shorter one is no warning: it is what a desktop with a faster display next to the one of the window gives.
Other programs used the CPUs during an idle run      | Find what was running and run the pair again.
Other programs did not use the CPUs during a loaded run | The load did not start, or the machine has more CPUs than the load uses.

## What the captures have shown

On a NVIDIA desktop GPU (driver 617.14) with a 240 Hz display on Windows 11, a window on the desktop.

### The system

- **Variable refresh changes what the display times mean.** With G-SYNC on the display followed the frames (its own counter showed the
  frame rate of the run) and the swapchain still reported a fixed refresh mode. The display time of a frame that waits before its
  present was reported before the present call. With G-SYNC off the display times were one refresh apart and after their present.
  The vertical blank the window system reports follows the frames as well then: its times were 1 to 7 refreshes apart at 60 fps
  and off the grid of the mode. The samples stop giving the pacer the vertical blank times once they see it.
- **The driver's refresh duration is not the refresh rate of the mode.** The swapchain reported 8.33 ms for every mode from 23.98 to
  120 Hz and 4.17 ms at 240 Hz. The rate of the window system was right at every mode, which is why the trace holds both. The machine
  has a second display at 120 Hz, and that turned out to be the reason: the swapchain reports the refresh of the fastest display of
  the desktop, whichever display the window is on (NVIDIA, `VK_EXT_present_timing`). With the displays at 240 and 120 Hz it is
  4.17 ms on both, at 60 and 120 Hz and at 50 and 120 Hz it is 8.33 ms, at 50 and 24 Hz it is 20.0 ms for a window on the 24 Hz
  display. A borderless full screen window gets the same. The frames go out on the refresh of the display the window is on: with it
  at 50 Hz all 745 display times of a run were 20.0 ms apart. Why the driver reports it this way is not known.
- **A present that is given a time is held twice as long on a display next to one at twice its rate.** With the window on a display
  at 60 Hz and a second display at 120 Hz, a frame of two refreshes was shown for four and a frame of three for six, in every
  frame, idle and under the CPU load. With both displays at 60 Hz, and with one display at 60 and at 50 Hz, the same frames were
  shown for their swap interval. It reads as the time being counted in the refreshes the swapchain reports and held in the
  refreshes of the display. A time the app waits until itself is not touched by it: it is counted in the rate of the window system.
- **A machine that is busy with something else looks like a pacing problem.** Uneven frame starts and a swap interval that cycled
  between one and two at 240 Hz went away when the builds in the background were stopped. That is why every run records the load of
  the machine and why a loaded run has an idle twin.
- **FIFO does not hold the frame loop to the display** (240 Hz, the pacer off, two swapchain images, one frame in flight).
  `vkAcquireNextImageKHR` returns in 0.002 ms and `vkQueuePresentKHR` in about 0.05 ms (the medians; 99 % of the presents return
  within 0.34 ms). The loop waits in one place, for the fence of the queue submit of the frame before, and that wait ends 0.05 ms
  after the GPU is done with that frame. With light work the display still paces the loop: the GPU starts a frame 3.2 ms after it
  was submitted, at the same place in every refresh, because the submit waits for the image. A present then reaches the display
  15.3 ms later (3.7 refreshes), more than two images in FIFO account for, so there is a queue below the swapchain. With GPU work
  of 3.83 ms in a refresh of 4.17 ms the image is free when the frame is submitted, the loop runs at the speed of the GPU (a frame
  every 4.10 ms), and 97 of 2340 presents got no display time while 135 frames were shown for two or three refreshes. In
  borderless full screen it is the same (88 and 105 presents without a display time at GPU work of 76 and 80 %). So when a frame
  starts and when it is presented are the pacer's to say, for every frame, see
  [FramePacing.md](FramePacing.md#what-a-vulkan-frame-loop-waits-on-and-why-the-pacer-gives-the-times), which also has what was
  found about the time from a present to the display.

### The pacer of the library

The pacer of mb-framepacing at `6482192`, every plan for 240 Hz in one session: 110 runs, 98 with the pacer on, variable refresh off
(read before every plan), High performance power plan. Every case is one run, idle and under the CPU load of the tool each once, so
no spread is known for a number below. Other programs used 2 to 8 % of the CPU in the runs without that load. The first 60 frames
of a run are left out. The runs ask for `timer-period` and `vblank-period` (no swapchain that can be waited on, no
`--Pacer.GpuWait`), the aim is smoothness.

- **A fixed frame rate is held, on a timer and on the vertical blank times** (`plain-vulkan-240hz` and `timed-present-240hz`).
  The frames that were shown for another number of refreshes than their swap interval, idle and under load: with `timer-period`
  at 120 fps 5 and 8 of 1138 (a second pair of runs: 0 of 2337 and 8 of 2338), at 60 fps 0 of 538 and 0 of 537 (a second pair: 0
  and 7 of 1138), at 30 fps 2 and 0 of 338. With `vblank-period` at 120 fps 0 and 3 of 1137, at 60 fps 0 and 3 of 537, at 30 fps
  0 and 3 of 337: the three of every loaded run were shown one refresh longer.
- **Work of 130 % of a refresh goes to two refreshes after 49 frames and stays there**: 18 of 18 runs, with both kinds, idle and
  under load, with and without present timing. The frame starts are 8.33 ms apart.
- **Work of 90 % of a refresh with one frame in flight is mostly held at one refresh.** 12 of 15 runs stayed at swap interval 1
  for the whole run, and the ten of them with present timing had 2 to 9 presents without a display time in nine runs and 21 in
  one. Three runs went to two refreshes for 156, 1301 and 1480 of 2400 frames. With the pacer off the same work had 42, 31 and 59
  presents without a display time.
- **Two frames in flight at work of 90 %: the pacer does not hold the loop.** The frame starts are 3.88 to 3.91 ms apart (the
  median of each of four runs) where a refresh is 4.166 ms, and 291 to 438 of 2400 presents got no display time, as with the
  pacer off (3.88 ms, 355). The swap interval stays 1. In the run that was looked into the pacer gave a time to wait until before
  the present for 17 of 2340 frames, where it gave one for 2122 with one frame in flight, and the loop was held by the wait for
  the frame slot, which is the GPU (3.48 ms, the median). Not run with `--Pacer.GpuWait` or with a wait for a present.
- **Where in a refresh a frame is ready** (`ready-place-sweep` at 120 fps, `--Pacer.ReadyPlace`). From 5 to 65 % no frame of 337
  was off its swap interval in 14 runs. At 75 % it was 0 idle and 2 under load, at 85 % 3 and 1: the present was made 0.79 ms
  before the frame reached the display there (the median). At 95 % the run paces at three refreshes per frame
  while the swap interval is two: all 339 frame start steps are three refreshes, the animation step is two, and the frames were
  shown for 2, 3 and 4 refreshes (44, 249 and 44 idle, 5, 327 and 5 under load). The present lay at 90 % of the refresh there and
  reached the display 1.09 refreshes later.
- **The present that takes a time on a timer** (`timed-present-240hz`, `--Pacer.TimedPresent` with `timer-period`). Idle no frame
  was off its swap interval at 60 fps (1137 frames) and at 120 fps (2337). Under the CPU load 141 of 1138 were at 60 fps (shown
  for 3, 4 and 5 refreshes: 71, 997 and 70) and 12 of 2338 at 120 fps, where the runs without the time had 7 and 8. In the run with
  141 the present was made at 88 % of a refresh, 0.60 ms before the frame reached the display; in its idle twin at 5 %. Where a timer lands
  in a refresh is chance, so one run does not say if it was the time given to the present.
- **FIFO latest ready at work of 90 %** (`present-options-240hz`, one run each): with the pacer on 132 presents without a display
  time and 125 frames shown for two refreshes, with it off 77 and 4.
