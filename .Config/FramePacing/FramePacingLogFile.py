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

# Reads the two files '--FramePacing.Log <file>' writes: '<file>' with one row for every frame and '<stem>.events.csv' with the facts of the run
# and what happened during it. Every value of a frame is a whole number, an empty field is a value that does not exist.

import csv
from dataclasses import dataclass
from pathlib import Path

g_ticksPerSecond = 10_000_000
g_ticksPerMillisecond = 10_000


@dataclass(frozen=True)
class LogEvent:
    FrameIndex: int
    TimeTicks: int
    Name: str
    Details: str

    def GetValues(self) -> dict[str, str]:
        """The details as 'key=value' pairs (the details of a 'column' event end with a free text that may hold a ';')"""
        result: dict[str, str] = {}
        for entry in self.Details.split(";"):
            key, separator, value = entry.partition("=")
            if separator != "":
                result[key] = value
        return result


class FramePacingLogFile:
    def __init__(self, framesPath: Path) -> None:
        self.FramesPath = framesPath
        self.EventsPath = ToEventsPath(framesPath)
        self.ColumnNames: list[str] = []
        self.RowCount = 0
        self.Events: list[LogEvent] = []
        self.Facts: dict[str, str] = {}
        self._columns: dict[str, list[int | None]] = {}
        self._ReadFrames()
        self._ReadEvents()

    def HasColumn(self, name: str) -> bool:
        """True if the column exists and at least one frame has a value in it"""
        return any(value is not None for value in self._columns.get(name, []))

    def GetColumn(self, name: str) -> list[int | None]:
        """One entry for every row, None where the frame has no value (all None if the column does not exist)"""
        column = self._columns.get(name)
        return column if column is not None else [None] * self.RowCount

    def GetValues(self, name: str) -> list[int]:
        """The values of the column that exist"""
        return [value for value in self.GetColumn(name) if value is not None]

    def GetEvents(self, name: str) -> list[LogEvent]:
        return [event for event in self.Events if event.Name == name]

    def _ReadFrames(self) -> None:
        with open(self.FramesPath, newline="", encoding="utf-8") as file:
            reader = csv.reader(file)
            self.ColumnNames = next(reader, [])
            columns: list[list[int | None]] = [[] for _ in self.ColumnNames]
            for row in reader:
                if len(row) != len(self.ColumnNames):
                    # A process that was killed can leave a last row that was not written completely
                    continue
                for column, field in zip(columns, row):
                    column.append(int(field) if field != "" else None)
                self.RowCount += 1
        self._columns = dict(zip(self.ColumnNames, columns))

    def _ReadEvents(self) -> None:
        if not self.EventsPath.is_file():
            return
        with open(self.EventsPath, newline="", encoding="utf-8") as file:
            reader = csv.reader(file)
            next(reader, None)
            for row in reader:
                if len(row) != 4:
                    continue
                event = LogEvent(int(row[0]), int(row[1]), row[2], row[3])
                self.Events.append(event)
                if event.Name == "fact":
                    key, _, value = event.Details.partition("=")
                    self.Facts[key] = value


def ToEventsPath(framesPath: Path) -> Path:
    """The name the app gives the events file of a log"""
    return framesPath.with_name(framesPath.stem + ".events.csv")
