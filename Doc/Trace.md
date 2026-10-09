# The trace service

`--Trace <file>` makes an app write a [Perfetto](https://perfetto.dev) trace: what its main thread was doing, and every value, event
and fact that is known about each frame. Open the file in [ui.perfetto.dev](https://ui.perfetto.dev) (it is read in the browser, it is
not uploaded), or query it with the Perfetto trace processor.

```bash
Vulkan.FramePacing --Pacer --Trace run.perfetto-trace --ExitAfterFrame 2000
```

A trace is written to be the base of a later analysis: another tool can read it, join it with what it measured itself by the frame
index and the run id, and add its own events to the file. What the trace holds is therefore a format with rules, see
[The format](#the-format).

Option | Description
---|---
`--Trace <file>` | Write the trace to the file.
`--Trace.Anonymise <on\|off>` | `on` (the default): the trace names the vendor of the graphics device in place of its model and placeholders in place of the directories of this machine. See [Anonymising](#anonymising).

The trace is written on Windows and Ubuntu. On the other platforms the service is there and records nothing, so an app does not
have to check for it. The trace is the log of the frames: what the frame pacing marker service, the host, the Vulkan app base and
the FramePacing samples know about a frame is recorded in it and nowhere else (see [FramePacing.md](FramePacing.md#the-frame-log)).

## What is in a trace

Track | What is on it
---|---
`Main` (a thread) | The zones of the main thread: what the thread did, as blocks inside blocks. See [the zones of the main thread](#the-zones-of-the-main-thread).
`Frames` | One span `Frame` for every frame, from where the host started on it to where the host was done with it. It carries **every value of the frame** as an argument, see below.
`Vulkan` | The calls and waits of a Vulkan app as spans of their frame: `wait for present`, `wait for frame slot`, `acquire`, `wait for acquire fence`, `submit`, `present`.
`Present to display` | Vulkan with present timing: a span from the present call of a frame to the first pixel out. The presents of the frames in flight overlap, so they are drawn in lanes.
`Display` | Vulkan with present timing: the marks `queue operations end`, `request dequeued`, `first pixel out` and `first pixel visible` of the present of a frame.
`Marker` | The mark `marker drawn` of the frame pacing marker.
`Pacer plan` | The marks `intended display` (an app with a pacer) and `next frame start` (the FramePacing samples).
`Sample frame`, `GPU` | The FramePacing samples: `wait for frame start`, `work` and `hold before present`, and the `GPU work` of a frame in lanes.
`CPU work`, `GPU time`, `Animation error`, `Swap interval` | The FramePacing samples: graphs with a point where each frame began.
`Events` | Something that happened at a moment: a swapchain was created, a setting changed. The details are arguments.
`Facts` | What holds for the whole run: one mark for each fact, named by its key, with the argument `value`.
`Schema` | What the frames hold, so a reader does not need the source that wrote it: a mark `value` for every value (`name`, `unit`, `description`), and `track`, `span`, `mark`, `counter` and `frameBounds` for what is drawn from them.

There are three kinds of things in a trace:

- A **zone** is a scope on the thread: it begins and ends where the code does, as often as it occurs, and zones are inside each other
  as the scopes are. A zone does not name a frame. The frame of a zone is the `Frame` span it is inside of in time.
- A **frame value** is a number of one frame: a time, a duration, a id, a result code. It is a argument of the `Frame` span of its
  frame, under its name, as the whole number it was set to. A value that the frame does not have is not there.
  [FramePacing.md](FramePacing.md#the-columns) lists and explains the values the framework and the FramePacing samples record.
- A **event** or a **fact**.

Some values are drawn as well: two times of a frame as a span, a time as a mark, a count as a graph. That is only for the eye. The
number to calculate with is the argument of the `Frame` span: a graph in a trace holds numbers with decimals, and a span is only
drawn when the frame has both of its times.

A frame is written 64 frames after it began, as some of its values arrive late (the display time of a present is reported a few
frames later). So a trace of a app that is running lacks its last 64 frames until the app exits, and a app that is killed loses them
and about a second of what came before.

### The zones of the main thread

The zones are layers: a zone is inside the zone of what called it. This is what a frame of a app looks like, from the host down to
the app. A zone that is indented is inside the one above it.

```
Native messages                        the messages of the window system, where the events of a frame come from
Host messages
Service messages
Update                                 the update of the frame
  PreUpdate                            a stage of the update. Every stage has the three zones below
    Extensions (before app)            the extensions of the app that run before it, the UI is one
      UI process events
    App                                the method of the app itself, with the zones the app adds
    Extensions (after app)
  FixedUpdate                          once for every fixed step that is due, so not in every frame
  App update
  PostUpdate
    Extensions (after app)
      UI update                        the layout and the animations of the UI
  Resolve
Prepare draw
  vkWaitForPresent2KHR                 Vulkan, if the swapchain waits for a earlier present
  Wait for frame slot                  Vulkan
  vkAcquireNextImageKHR                Vulkan
  Wait for acquire fence               Vulkan, if the swapchain waits for it
Draw
  BeginDraw
  App draw                             the draw method of the app, with the zones the app adds
    Record commands                    Vulkan: the app records the commands of the frame
      UI draw
        UI pre draw
        UI render
          UI preprocess draw commands  the steps of the render system of the UI
          UI generate meshes
          UI update buffers
          UI schedule draw
        UI post draw
      Profiler draw                    Vulkan: the overlay of the profiler, if it is shown
      Marker draw                      Vulkan: the frame pacing marker
    Submit frame                       Vulkan
      vkQueueSubmit
  Marker draw                          the frame pacing marker, where the host draws it (not Vulkan)
  EndDraw
  Profiler draw                        the overlay of the profiler, if it is shown (not Vulkan)
Swap                                   the frame is handed over
  App swap                             Vulkan: the app presents
    vkQueuePresentKHR
```

The FramePacing samples add what they do in a frame:

Where | Zones
---|---
In their update | `Keyboard menu`, `Pacer update`, `Tier update`, `Stats UI`, `Work chart`, `Animation error`, `Run UI`
Where the frame starts (the update of a GLES sample, the draw of the Vulkan sample) | `Start frame`, with `Wait for frame start` and `CPU load` inside it
In their draw | `GPU timer`, `Measurements` (Vulkan), `Background draw`, `Sample draw` with `Draw animation`, `Draw box animation` and `UI draw` inside it, `Flush` (GLES with `--GLFlush`)
After their draw | `End frame`, `Wait for present` with `Hold before present` inside it

- A wait is only a zone when there was something to wait for, so `Wait for frame start` and `Hold before present` are not in
  every frame.
- The UI library does not know the trace. The steps of its render system are zones because the render system times them itself,
  and they are added with those times when the draw is done. The walk of the window tree before them is the time of `UI render`
  that none of its steps cover.
- The zones are recorded by `ScopedTraceZone` objects in the code, so the list above is what the code has zones for and not what
  a thread can do. A app adds its own as the example below shows.

## Recording from an app

An app, a host or a service records through `ITraceService`. Every call does nothing while the trace is off, so the only check that
is needed is the one for the service. No Perfetto type is part of the interface.

```C++
#include <FslDemoService/Trace/ITraceService.hpp>
#include <FslDemoService/Trace/ScopedTraceZone.hpp>

// In the constructor: the names are added once. Values and tracks can be added until the first frame is written
m_trace = config.DemoServiceProvider.TryGet<ITraceService>();
if (m_trace)
{
  m_zonePhysics = m_trace->RegisterZone("Physics");
  m_valueBodies = m_trace->RegisterValue("physicsBodies", TraceUnit::Count, "The bodies that were simulated for the frame");
  m_valueUploadBegin = m_trace->RegisterValue("uploadBeginTicks", TraceUnit::Ticks, "When the upload of the frame began");
  m_valueUploadEnd = m_trace->RegisterValue("uploadEndTicks", TraceUnit::Ticks, "When the upload of the frame was done");
  // How it is drawn
  const TraceTrack track = m_trace->RegisterTrack("Upload", TraceTrackKind::Lanes);
  m_trace->DeclareSpan("upload", track, m_valueUploadBegin, m_valueUploadEnd, TraceLink::NoLink);
  m_trace->DeclareCounter("Bodies", m_valueBodies);
  m_trace->SetFact("scene", "city");
}

// A zone lasts as long as the object
{
  const ScopedTraceZone zone(m_trace.get(), m_zonePhysics);
  RunPhysics();
}

// During the draw of a frame: a value of the frame that is being drawn
if (m_trace)
{
  m_trace->SetInt64(m_valueBodies, bodyCount);
  // A time or a duration is given as the type it is (TickCount, TimeSpan, NanosecondTickCount, NanosecondTimeSpan): the trace writes
  // it in the unit of the value. A value that is known later is set for the frame it belongs to
  m_trace->SetValueAt(frameIndex, m_valueUploadEnd, uploadEndTime);
  m_trace->AddEvent("levelLoaded", "name=city");
}
```

- **Which to use.** A scope of the code on the thread is a zone. Something that is not one scope, or does not happen on the thread
  (the work of the GPU, the way of a present to the display), is two time values that are declared as a span.
- **Tracks.** The spans of a `Sequential` track follow each other or are inside each other. The spans of a `Lanes` track can overlap
  from frame to frame and are drawn below each other. A span that cuts through another one on a `Sequential` track is drawn on a
  lane below it and gets the argument `overflow`.
- **The chain of a frame.** A span or a mark that is declared with `TraceLink::FrameChain` is a step of its frame. A viewer draws
  the steps of a frame linked in the order of their times: the frame, its submit, the work of the GPU, its present, its first pixel.
- **The thread.** The service records on the thread it was created on, the main thread of the app. The id the system has for the
  thread is read once and kept. Other threads can not record yet.
- **The cost.** A zone is a read of the clock and a store. A value is a store. The trace is written by a thread of its own: no call
  of the Perfetto SDK is made on the thread that records.
- **The frames.** A frame begins where the frame pacing marker service begins it, so the frame index and the run id of the trace are
  those the marker of the frame carries (`ITraceServiceControl::BeginFrame`). On a platform without that service no frame begins.

## The format

What another tool can rely on. `trace.formatVersion` is a fact of every trace, it is `1` for what is described here and changes when
one of these rules or one of the names does.

1. **The clock.** Every event is stamped with the monotonic clock of the system (`BUILTIN_CLOCK_MONOTONIC`), which is the clock of
   the framework (`HighResolutionTimer`: `QueryPerformanceCounter` on Windows, `CLOCK_MONOTONIC` on Linux), and it is the primary
   clock of the trace. A time of the trace in nanoseconds is a tick of the framework times 100, exactly. The facts
   `trace.utcNanoseconds` and `trace.utcClockTicks` are the same moment as a wall clock time and as a tick.
2. **A plain file.** The trace has no service events, is not compressed and not filtered. It is a row of trace packets from its
   first byte to its last.
3. **The tracks have names.** Every track has a descriptor with its name, the names are the ones of the table above. The id of a
   track is not part of the format: a tool finds a track by its name. The lanes of a track are tracks of the same name.
4. **A frame is found by its frame index and its run id.** The `Frame` span and every span and mark of a frame have the arguments
   `frameIndex` and `runId`. They are the frame index and the run id of the frame pacing marker of the frame, which is what a capture
   of the display is decoded to.
5. **The chain of a frame.** The steps of a frame have the flow id `(runId << 32) | (frameIndex & 0xFFFFFFFF)`, and the chain is
   not ended. A tool that gives one of its events that flow id adds a step to the chain of the frame.
6. **The names.** The names of the values are those of the `Schema` track, with their units. A value of the unit `ticks` is a
   moment and `durationTicks` a duration, both in ticks of 100 nanoseconds. `nanosecondTicks` is a moment on the same clock in
   nanoseconds, which can be drawn as a mark. `nanoseconds` is a duration in nanoseconds, or a moment in nanoseconds on a clock that
   is not the one of the framework (a driver and the window system have a clock of their own). A app does not convert between ticks
   and nanoseconds: `SetValue` takes a moment as a `TickCount` or a `NanosecondTickCount` and a duration as a `TimeSpan` or a
   `NanosecondTimeSpan`, and the trace writes it in the unit the value was registered with (a moment as the tick it lies in, a
   duration rounded to the nearest tick). A moment given to a value that is not a moment, or a duration to one that is not a
   duration, is not written and logged once. A unsigned value is written as its 64 bits. The `value` marks of the `Schema` track are in the order the values
   were added, which is the order of the columns where the values are written as a table.

### Reading a trace

`.Config/FramePacing/FramePacingTraceFile.py` reads a trace into rows of values, events and facts for the Python tools of the
framework, the capture tool checks a run with it (see [FramePacingCapture.md](FramePacingCapture.md)). A tool that reads the CSV
files an app used to write as its frame pacing log gets them from a trace with `.Config/FramePacing/FramePacingTraceToCsv.py`.

With the trace processor (the `perfetto` package of Python, the `trace_processor` shell or the query page of the viewer), the frames
with a few of their values:

```sql
select extract_arg(s.arg_set_id, 'debug.frameIndex') as frameIndex,
       extract_arg(s.arg_set_id, 'debug.runId') as runId,
       extract_arg(s.arg_set_id, 'debug.presentCallTicks') as presentCallTicks,
       extract_arg(s.arg_set_id, 'debug.firstPixelOutTicks') as firstPixelOutTicks
from slice s join track t on s.track_id = t.id
where s.name = 'Frame' and t.name = 'Frames'
order by frameIndex
```

Every value of every frame (`key` is `debug.<name>`), and the facts:

```sql
select extract_arg(s.arg_set_id, 'debug.frameIndex') as frameIndex, a.key, a.int_value
from slice s join track t on s.track_id = t.id join args a on a.arg_set_id = s.arg_set_id
where s.name = 'Frame' and t.name = 'Frames';

select s.name as key, extract_arg(s.arg_set_id, 'debug.value') as value
from slice s join track t on s.track_id = t.id where t.name = 'Facts';
```

### Adding to a trace

A tool adds events of its own by appending trace packets to a copy of the file. This was tried with a script that added one event
for each of the 600 frames of a trace: the result loaded without errors, every added event was at the time it was given, was a step
of the chain of its frame, and its track was under the process of the app.

1. Append to a copy, and only to the trace of a app that has exited.
2. Use a `trusted_packet_sequence_id` no packet of the file has, and set `SEQ_INCREMENTAL_STATE_CLEARED` on the first packet.
3. Give the tracks of the tool ids of its own. A track is put under the process of the app by setting `parent_uuid` to the `uuid`
   of the track descriptor of the file that has a `process`.
4. Stamp the events with `timestamp_clock_id = 3` (`BUILTIN_CLOCK_MONOTONIC`) and a time in nanoseconds on the clock of the trace.
   The tool converts its own times to that clock itself. The frames of the trace and what the tool measured for the same frame index
   are what relates the two clocks.
5. Give a event of a frame the arguments `frameIndex` and `runId`, and the flow id of rule 5 to make it a step of the frame.

## Anonymising

A trace is made to be handed on, and the model of somebody's graphics device or the name of their home directory is not something
to hand on by accident. With `--Trace.Anonymise on`, which is the default:

- The model of the graphics device is replaced by its vendor (`NVIDIA GPU`) wherever a event or a fact names it, and the fact
  `vulkan.deviceId` is `0x0`. The vendor id and the driver version stay.
- The directory of the trace file, the home directory and the directory of the SDK are replaced by `<output>`, `<home>` and `<sdk>`
  in every event and fact.
- The command line of the process is not written.

The fact `trace.anonymised` says which it was. A producer that writes what names the hardware tells the service with
`AddAnonymousText` or `AddAnonymousFact`, as the Vulkan app base does. What an app prints is not changed by this option, the capture
tool anonymises that.

## Implementation

Package | Content
---|---
`ThirdParty/perfetto` | The Perfetto C++ tracing SDK (via `Recipe.perfetto_58_2`), built with a CMake script of the recipe as the SDK is released as two files without one.
`FslDemoService.Trace` | The public `ITraceService` interface, its types (`TraceZone`, `TraceValue`, `TraceTrack`, `TraceFrameIndex`, `TraceRunId`, `TraceUnit`, `TraceTrackKind`, `TraceLink`) and `ScopedTraceZone` (header only, available on all platforms).
`FslDemoService.Trace.Control` | The `ITraceServiceControl` interface of the one that begins the frames (header only, available on all platforms).
`FslDemoService.Trace.Impl` | The service, its command line options, the table of the open frames, the zones that wait, the anonymiser and the writer thread. It writes through `ITraceSink` and has no Perfetto dependency (available on all platforms).
`FslDemoService.Trace.Perfetto` | The sink that writes the Perfetto trace. It is the only package that includes the SDK (Windows and Ubuntu).

The service is registered by `FslDemoPlatform` on every platform, with the Perfetto sink where its package is supported. Hosts and apps
only use the interfaces, which they get with `TryGet`.

The frame pacing marker service, the host (`DemoAppManager`), the Vulkan app base and the FramePacing samples record what they know
about a frame with `ITraceService`. The marker service begins the frames of the trace and sets their bounds through
`ITraceServiceControl`, so a frame of the trace is the frame its marker carries.
