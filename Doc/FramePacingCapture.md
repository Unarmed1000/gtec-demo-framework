# Capturing frame pacing logs

How to record what an app did with its frames, in a way that can be repeated: the same runs, the same load, and notes that say what the
machine was doing. The log itself is written by the app (`--FramePacing.Log <file>`, see [FramePacing.md](FramePacing.md#the-frame-log)),
the capture tool runs the app once for every run of a plan and checks each log.

The tool is in `.Config/FramePacing`:

File                        | What it is
----------------------------|----------------------------------------------------------------------------------------------------------
`FramePacingCapture.py`     | The capture tool. After `prepare` it is `FramePacingCapture` on Windows and `FramePacingCapture.sh` elsewhere.
`CpuLoad.py`                | The external CPU load of a loaded run. It can be used on its own.
`FramePacingRunCheck.py`    | The checks of a log.
`FramePacingLogFile.py`     | Reads the two files of a log, for scripts of your own.
`Plans/*.toml`              | The plans that come with the tool.

It needs Python 3.11 or newer and nothing outside the standard library.

## Before a capture

A frame pacing log is only worth something if it is known what the machine and the display were doing. The tool records what it can
read, the rest is up to the person at the machine.

- **Build the app as a release build.** The tool runs the newest release build of the plan's app under `FSL_GRAPHICS_SDK/build`
  (`Vulkan.FramePacing` for the plans that come with the tool), or the one given with `--exe`.
- **Set the refresh rate of the display** to the one the plan is for. The window system's rate is written to the log and a run is checked
  against the rate of the plan (or `--refresh-hz`).
- **Decide on variable refresh (G-SYNC, FreeSync)** and say so with `--fact`. No API tells an app if it is on: with it on the display
  follows the frames, the swapchain still reports a fixed refresh mode, and the display times of a run look nothing like the ones with
  it off. The frame pacer needs it off.
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
- **Leave the keyboard and the mouse alone during the runs.** The window of the app takes the keyboard focus when it opens and the
  sample reacts to keys: **P** toggles the pacer. A run where the pacer was switched gets a warning. Do not move or resize the window, a
  run with more than one swapchain gets a warning as well.
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
`present-feedback-240hz` | 240 Hz, variable refresh off | The pacer without and with present feedback (`--Pacer.PresentFeedback`), GPU work of 20, 90 and 130 %, each idle and under CPU load (12 runs).
`present-feedback-120hz` | 120 Hz, variable refresh off | The same with CPU work of 2, 7 and 11 ms.
`plain-vulkan-240hz`, `-120hz`, `-60hz`, `-50hz` | that rate, variable refresh off | Does the pacer work on plain Vulkan: a frame held by a timer sleep and by a wait on the vsync (`--Pacer.Hold`) at fixed frame rates and under work of 130 %, one and two frames in flight, and the key rows with present timing off as well (22 to 30 runs).
`present-scheduling-240hz` | 240 Hz, variable refresh off | A frame held for more than one refresh by a wait before the present and by a scheduled present (`--Pacer.Hold schedule`): a fixed 60 and 120 fps and work of 130 %, each idle and under CPU load (12 runs).
`present-options-240hz` | 240 Hz, variable refresh off | The swapchain setup at work of 90 %: one against two frames in flight (`--VkFramesInFlight`) and FIFO against FIFO latest ready (`--VkPresentMode`), pacer off and on (8 runs).
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

The tool adds `--FramePacing.Log <output>/<name>.csv --ExitAfterFrame <frames> -v` to every run.

## What a capture leaves behind

File                   | Content
-----------------------|------------------------------------------------------------------------------------------------------------
`<run>.csv`            | The frames of the run, one row per frame. See [the frame log](FramePacing.md#the-frame-log).
`<run>.events.csv`     | The facts of the run and what happened during it.
`<run>.run.txt`        | The notes of the run: what it was, the command line, when it ran, the load, what you told with `--fact`, the machine (OS, CPU, power plan, display mode, GPU and driver, swapchain, pacer settings) and the checks.
`<run>.app.log`        | What the app printed.
`summary.txt`          | One line per run, with its warnings.

Keep the directory together, the notes are what makes a log usable later.

A capture does not name the hardware it was made on. The log of an app and what it prints name the model of the graphics device, and
the tool replaces it in the files of every run with the vendor (`NVIDIA GPU`), sets the device id to zero and leaves the model of the
CPU out of the notes. The vendor and the driver version stay, they are what a reader of a log needs. `--hardware-names` keeps the
models. A log that an app wrote without the tool (`--FramePacing.Log`) names the device as the driver reports it.

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
`presents not shown or not timed` | Presents the presentation engine reported on without a display time: their image did not reach the display.
`feedback used: n, refused: n` | With present feedback: what became of the display times the pacer was given. Many refused is a warning: variable refresh, or a wrong refresh rate.
`work`                         | The median CPU and GPU time of a frame.
`other programs`               | The share of the CPUs that was busy with something other than the app during the run.
`ok` or `n WARNINGS`           | The warnings are listed below the line and in the notes.

The first 60 frames of a run are left out of the times, the swapchain, the pacer and the GPU timer are still settling there. The numbers
are a first look that tells if a run is usable and roughly what it showed. They are made from the log, which holds every value.

Logs that exist already can be checked again:

```bash
FramePacingCapture check D:/captures/240hz/w90_pacer_on_idle.csv
FramePacingCapture check --write-notes --refresh-hz 240 D:/captures/240hz/*.csv
```

### The warnings

Warning                                              | What to do
-----------------------------------------------------|-----------------------------------------------------------------------------
The app exited with a code, or logged fewer frames   | Look at `<run>.app.log`.
More than one swapchain was created                  | The window was resized, moved or lost. Run it again.
The pacer was switched during the run                | A key reached the window. Run it again.
The display runs at another rate than the plan is for| Set the display mode, or give the rate with `--refresh-hz`.
The refresh rate changed during the run              | The window was moved to another display, or the mode was changed. Run it again.
Only some presents have a display time               | The presents that were reported without one did not reach the display. Compare with the idle twin: under load it is a finding, not a fault of the run.
Other programs used the CPUs during an idle run      | Find what was running and run the pair again.
Other programs did not use the CPUs during a loaded run | The load did not start, or the machine has more CPUs than the load uses.

## What the captures so far have shown

On a NVIDIA desktop GPU (driver 617.14) with a 240 Hz display on Windows 11, a window on the desktop:

- **Variable refresh changes what the display times mean.** With G-SYNC on the display followed the frames (its own counter showed the
  frame rate of the run) and the swapchain still reported a fixed refresh mode. The display time of a frame whose present the sample
  delays was reported before the present call. With G-SYNC off the display times were one refresh apart and after their present.
- **The driver's refresh duration is not the refresh rate of the mode.** The swapchain reported 8.33 ms for every mode from 23.98 to
  120 Hz and 4.17 ms at 240 Hz. The rate of the window system was right at every mode, which is why the log holds both.
- **A machine that is busy with something else looks like a pacing problem.** Uneven frame starts and a swap interval that cycled
  between one and two at 240 Hz went away when the builds in the background were stopped. That is why every run records the load of
  the machine and why a loaded run has an idle twin.
- **On an idle machine the pacer held one refresh** at 23.98, 24, 25, 29.97, 60, 100, 120 and 240 Hz with trivial work, with frame starts
  within about 0.15 ms of the refresh interval, and a frame reached the display about four refreshes after it started.
- **Work over a refresh did not make the pacer slow down** with mb-framepacing `11aeb1ae` (the work matrix at 240 Hz with GPU work and
  at 120 Hz with CPU work). With work of 130 % of a refresh it held swap interval 1 for the whole run, idle and loaded, with and
  without present timing: the frames started 1.37 refreshes apart, were shown for one or two refreshes in turn and the animation ran at
  about 73 % of real time, the same picture as with the pacer off. Its frame window held the right average work and next to no late
  frames. The frame loop is not blocked at a vsync while the swapchain has room, so a frame that is too long does not start two
  refreshes after the one before it, and the pacer only counted a frame as late from the time between two frame starts.
- **With mb-framepacing `1764848` it does**: a frame whose work is longer than its frame time counts as late. The same runs go to two
  refreshes after 25 frames at 120 Hz and after 49 frames at 240 Hz and stay there, with frame starts exactly two refreshes apart and
  every frame shown for two refreshes. Nothing but the pinned commit changed, which is what the plans are for: the same runs before
  and after a change.
- **Work of 90 % of a refresh at 240 Hz is decided by chance**: the work fits, some frames miss their refresh anyway, and whether the
  pacer goes to two refreshes depends on how many do. In one run it switched after five seconds in which 9 % of the frames were shown
  too long, in others it held one refresh to the end with 2 % shown too long. At 120 Hz (84 %) it held one refresh with next to no
  frames shown too long.
- **With mb-framepacing `19acdf7` present feedback no longer delays the slow down**, and a present without a display time is
  reported to the pacer as not shown. The same plans again, idle and under the CPU load: with work of 130 % the pacer goes to two
  refreshes at frame 52 at 240 Hz and at frame 27 at 120 Hz, two or three frames after the run without feedback (49 and 25), and it
  stays at two. With work of 90 % at 240 Hz on a idle machine the pacer with feedback went to two refreshes after 466 frames, where the
  one without stayed at one refresh and showed 115 of 2400 frames for two or three refreshes; under the CPU load both stayed at one
  refresh. With light work the runs with and without it are the same. These are the summaries of the runs, the logs have not been
  read frame by frame. It stays off by default.
- **Present feedback had not helped before that** (mb-framepacing `1764848`, the present feedback plans at 120 and 240 Hz, idle, under the CPU
  load and under builds that used up to 83 % of the CPUs). With light work the runs with and without it are the same: the frame starts
  stayed within 0.2 ms of the refresh interval under every load, so there was nothing to correct. With work of 90 % at 240 Hz the
  frames that miss their refresh are reported without a display time, so the pacer gets no feedback for them and counts them as on
  time. With work of 130 % the pacer takes about three times as long to go to two refreshes, and in two runs it went on to three and
  four.
- **A CPU load on every logical CPU did not disturb the frames of the sample**, the runs under load were as even as their idle twins.
  A build loads the disk and the memory as well, which this load does not.
- **The time from the start of a frame to the display depends on the work**: 3.7 refreshes at 20 % work and about 2 refreshes at 90
  and 130 %.
