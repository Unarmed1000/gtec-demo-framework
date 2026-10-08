#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2026, Mana Battery ApS
# * All rights reserved.
# *
# * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
# *
# * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
# * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
# *    documentation and/or other materials provided with the distribution.
# * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
# *    software without specific prior written permission.
# *
# * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# ****************************************************************************************************************************************************

# Writes the two CSV files of a frame pacing log from the trace '--Trace <file>' writes, for a tool that reads the CSV files:
# '<name>.csv' with one row for every frame and '<name>.events.csv' with the facts, the columns and the events. The format is version 1
# of the frame pacing log, see Doc/FramePacing.md.
#
#   FramePacingTraceToCsv.py <run>.perfetto-trace                 writes <run>.csv and <run>.events.csv next to the trace
#   FramePacingTraceToCsv.py <run>.perfetto-trace -o <file.csv>   writes <file.csv> and <file>.events.csv
#
# The files are those an app wrote itself ('--FramePacing.Log <file>') before the trace was its log. What is not as it was there: a
# value is written as a signed number, and the facts are those of the trace ('trace.' facts included).

import argparse
import csv
import sys
from pathlib import Path

from FramePacingLogFile import LogEvent, ToEventsPath, g_traceFileSuffix
from FramePacingTraceFile import FramePacingTraceFile, TraceReadError

# The version of the format of the CSV files that is written here
_g_csvFormatVersion = 1
# How many frames a frame stays open for values that are known later: the fact of the CSV log, and the one the trace has for it
_g_csvOpenFramesFact = "log.openFrames"
_g_traceOpenFramesFact = "trace.openFrames"
_g_eventsHeader = ["frameIndex", "timeTicks", "event", "details"]


def _ToDefaultOutputPath(tracePath: Path) -> Path:
    name = tracePath.name.removesuffix(g_traceFileSuffix) if tracePath.name.endswith(g_traceFileSuffix) else tracePath.stem
    return tracePath.with_name(name + ".csv")


def WriteFrames(trace: FramePacingTraceFile, framesPath: Path) -> None:
    columns = [trace.GetColumn(name) for name in trace.ColumnNames]
    # The line ends are those the app writes, on every system
    with open(framesPath, "w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, lineterminator="\n")
        writer.writerow(trace.ColumnNames)
        for row in range(trace.RowCount):
            writer.writerow(["" if column[row] is None else column[row] for column in columns])


def WriteEvents(trace: FramePacingTraceFile, eventsPath: Path) -> None:
    firstTime = min((event.TimeTicks for event in trace.Events), default=trace.ValuesTimeTicks)
    events: list[LogEvent] = [LogEvent(0, firstTime, "fact", f"formatVersion={_g_csvFormatVersion}")]
    if _g_csvOpenFramesFact not in trace.Facts and _g_traceOpenFramesFact in trace.Facts:
        events.append(LogEvent(0, firstTime, "fact", f"{_g_csvOpenFramesFact}={trace.Facts[_g_traceOpenFramesFact]}"))
    events.extend(trace.Events)
    # What the columns are is an event for each of them, as the frames file is a plain table
    events.extend(LogEvent(0, trace.ValuesTimeTicks, "column", f"name={value.Name};unit={value.Unit};description={value.Description}")
                  for value in trace.Values)
    # A stable sort: what has the same time stays in the order it was added
    events.sort(key=lambda event: event.TimeTicks)
    with open(eventsPath, "w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, lineterminator="\n")
        writer.writerow(_g_eventsHeader)
        for event in events:
            writer.writerow([event.FrameIndex, event.TimeTicks, event.Name, event.Details])


def _CreateParser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Writes the CSV files of a frame pacing log from a trace of the trace service.")
    parser.add_argument("trace", help="The trace of a run (--Trace <file>).")
    parser.add_argument("-o", "--output", help="The frames file to write (default: the name of the trace with .csv, next to the trace). "
                                               "The events file is written next to it as <name>.events.csv.")
    parser.add_argument("--overwrite", action="store_true", help="Replace the files if they exist.")
    return parser


def Main(argv: list[str]) -> int:
    args = _CreateParser().parse_args(argv)
    tracePath = Path(args.trace)
    if not tracePath.is_file():
        print(f"ERROR: '{tracePath}' is not a file", file=sys.stderr)
        return 2
    framesPath = Path(args.output) if args.output is not None else _ToDefaultOutputPath(tracePath)
    eventsPath = ToEventsPath(framesPath)
    existing = [path for path in (framesPath, eventsPath) if path.exists()]
    if len(existing) > 0 and not args.overwrite:
        print(f"ERROR: {' and '.join(str(path) for path in existing)} exist already (use another --output, or --overwrite)", file=sys.stderr)
        return 2
    try:
        trace = FramePacingTraceFile(tracePath)
    except TraceReadError as ex:
        print(f"ERROR: {ex}", file=sys.stderr)
        return 2
    WriteFrames(trace, framesPath)
    WriteEvents(trace, eventsPath)
    print(f"{framesPath}: {trace.RowCount} frames, {len(trace.ColumnNames) - 1} values")
    print(f"{eventsPath}: {len(trace.Events) + len(trace.Values) + 1} events")
    for problem in trace.ReadProblems:
        print(f"WARNING: {problem}", file=sys.stderr)
    return 0 if len(trace.ReadProblems) == 0 else 1


if __name__ == "__main__":
    sys.exit(Main(sys.argv[1:]))
