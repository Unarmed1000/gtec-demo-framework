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

# Reads the trace '--Trace <file>' writes (see Doc/Trace.md) as a FramePacingLog: the values of the 'Frame' spans are the rows, the 'Events'
# track the events and the 'Facts' track the facts. It is read with the trace processor of Perfetto, which the 'perfetto' package of Python
# brings (pip install perfetto). The package downloads the trace processor the first time it is used.

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from FramePacingLogFile import FramePacingLog, LogEvent, g_frameIndexColumnName

# The names of the format, see 'The format' in Doc/Trace.md
_g_argumentPrefix = "debug."
_g_framesTrackName = "Frames"
_g_frameSpanName = "Frame"
_g_eventsTrackName = "Events"
_g_factsTrackName = "Facts"
_g_schemaTrackName = "Schema"
_g_schemaValueName = "value"
_g_formatVersionFact = "trace.formatVersion"
_g_supportedFormatVersion = "1"
_g_nanosecondsPerTick = 100


class TraceReadError(Exception):
    pass


@dataclass(frozen=True)
class TraceValueInfo:
    Name: str
    Unit: str
    Description: str


def _CreateTraceProcessor(tracePath: Path) -> Any:
    try:
        from perfetto.trace_processor import TraceProcessor, TraceProcessorConfig
    except ImportError as ex:
        raise TraceReadError(f"A trace is read with the 'perfetto' package of Python, which is not installed: pip install perfetto ({ex})") from ex
    try:
        return TraceProcessor(trace=str(tracePath), config=TraceProcessorConfig())
    except Exception as ex:
        raise TraceReadError(f"The trace '{tracePath}' could not be opened: {ex}") from ex


def _SelectTrack(columns: str, trackName: str, where: str = "") -> str:
    """The slices of the tracks of a name (the lanes of a track are tracks of the same name)"""
    return (f"select {columns} from slice s join track t on s.track_id = t.id where t.name = '{trackName}'{where} order by s.ts, s.id")


class FramePacingTraceFile(FramePacingLog):
    def __init__(self, tracePath: Path) -> None:
        super().__init__([tracePath])
        self.TracePath = tracePath
        # The values a frame can have, in the order they were added by the app, and when the trace was told about them
        self.Values: list[TraceValueInfo] = []
        self.ValuesTimeTicks = 0
        self._factEvents: list[LogEvent] = []
        traceProcessor = _CreateTraceProcessor(tracePath)
        try:
            self._ReadProblems(traceProcessor)
            self._ReadSchema(traceProcessor)
            self._ReadFrames(traceProcessor)
            self._ReadFacts(traceProcessor)
            self._ReadEvents(traceProcessor)
        finally:
            traceProcessor.close()
        formatVersion = self.Facts.get(_g_formatVersionFact)
        if formatVersion != _g_supportedFormatVersion:
            self.ReadProblems.append(f"the trace says its format version is {formatVersion}, this tool reads version {_g_supportedFormatVersion}")

    def _ReadProblems(self, traceProcessor: Any) -> None:
        for row in traceProcessor.query("select name, severity, value from stats where severity in ('error', 'data_loss') and value > 0"):
            self.ReadProblems.append(f"the trace processor reports {row.name}={row.value} ({row.severity}) for the trace")

    def _ReadSchema(self, traceProcessor: Any) -> None:
        arguments = self._ReadArguments(traceProcessor, _g_schemaTrackName, f" and s.name = '{_g_schemaValueName}'")
        for row in traceProcessor.query(_SelectTrack("s.ts as ts, s.arg_set_id as arg_set_id", _g_schemaTrackName,
                                                     f" and s.name = '{_g_schemaValueName}'")):
            self.ValuesTimeTicks = row.ts // _g_nanosecondsPerTick
            values = arguments.get(row.arg_set_id, {})
            name = values.get("name")
            if isinstance(name, str) and name != "":
                self.Values.append(TraceValueInfo(name, str(values.get("unit", "")), str(values.get("description", ""))))
        self.ColumnNames = [g_frameIndexColumnName] + [value.Name for value in self.Values]

    def _ReadFrames(self, traceProcessor: Any) -> None:
        # Every value of a frame is an argument of its 'Frame' span
        frames: dict[int, dict[str, int]] = {}
        arguments = self._ReadArguments(traceProcessor, _g_framesTrackName, f" and s.name = '{_g_frameSpanName}'", True)
        for values in arguments.values():
            frameIndex = values.get(g_frameIndexColumnName)
            if not isinstance(frameIndex, int):
                self.ReadProblems.append("a frame of the trace does not say which frame it is, it was left out")
                continue
            if frameIndex in frames:
                self.ReadProblems.append(f"frame {frameIndex} is in the trace more than once, the last one was used")
            frames[frameIndex] = {name: value for name, value in values.items() if isinstance(value, int)}

        frameIndices = sorted(frames)
        self.RowCount = len(frameIndices)
        columns: dict[str, list[int | None]] = {name: [None] * self.RowCount for name in self.ColumnNames}
        for row, frameIndex in enumerate(frameIndices):
            values = frames[frameIndex]
            for name in self.ColumnNames:
                columns[name][row] = values.get(name)
        self._columns = columns

    def _ReadFacts(self, traceProcessor: Any) -> None:
        arguments = self._ReadArguments(traceProcessor, _g_factsTrackName)
        for row in traceProcessor.query(_SelectTrack("s.name as name, s.ts as ts, s.arg_set_id as arg_set_id", _g_factsTrackName)):
            value = arguments.get(row.arg_set_id, {}).get("value", "")
            self.Facts[row.name] = str(value)
            self._factEvents.append(LogEvent(0, row.ts // _g_nanosecondsPerTick, "fact", f"{row.name}={value}"))

    def _ReadEvents(self, traceProcessor: Any) -> None:
        arguments = self._ReadArguments(traceProcessor, _g_eventsTrackName)
        for row in traceProcessor.query(_SelectTrack("s.name as name, s.ts as ts, s.arg_set_id as arg_set_id", _g_eventsTrackName)):
            values = arguments.get(row.arg_set_id, {})
            # An event from before the first frame names no frame
            frameIndex = values.get(g_frameIndexColumnName, 0)
            self.Events.append(LogEvent(frameIndex if isinstance(frameIndex, int) else 0, row.ts // _g_nanosecondsPerTick, row.name,
                                        str(values.get("details", ""))))
        # The facts are events of a frame pacing log as well, so they are here too: a tool that looks for an event finds the same ones
        self.Events = sorted(self._factEvents + self.Events, key=lambda event: event.TimeTicks)

    @staticmethod
    def _ReadArguments(traceProcessor: Any, trackName: str, where: str = "", wholeNumbersOnly: bool = False) -> dict[int, dict[str, int | str]]:
        """The arguments of the slices of a track, for each set of arguments by their name"""
        result: dict[int, dict[str, int | str]] = {}
        query = ("select a.arg_set_id as arg_set_id, a.key as key, a.int_value as int_value, a.string_value as string_value from slice s "
                 f"join track t on s.track_id = t.id join args a on a.arg_set_id = s.arg_set_id where t.name = '{trackName}'{where}")
        for row in traceProcessor.query(query):
            if not row.key.startswith(_g_argumentPrefix):
                continue
            value: int | str | None = row.int_value if row.int_value is not None else (None if wholeNumbersOnly else row.string_value)
            if value is not None:
                result.setdefault(row.arg_set_id, {})[row.key[len(_g_argumentPrefix):]] = value
        return result
