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


import json
import os
import re
from dataclasses import dataclass, field
from typing import Any, final

from FslBuildGen import IOUtil
from FslBuildGen.Log import Log

_FORMAT_VERSION = 1

# A preprocessor include like '#include "Common.glsl"' or '#  include <Common.glsl>'
_INCLUDE_PATTERN = re.compile(rb"^[ \t]*#[ \t]*include\b", re.MULTILINE)


def ContainsInclude(filename: str) -> bool:
    """True if the file has a preprocessor '#include' line"""
    with open(filename, "rb") as theFile:
        return _INCLUDE_PATTERN.search(theFile.read()) is not None


@final
@dataclass(frozen=True)
class DependencyState:
    """A file a content output depends on, beyond the content file it was built from"""

    Path: str
    Length: int
    ModifiedTime: int
    Checksum: str


@final
@dataclass(frozen=True)
class OutputDependencies:
    """The dependencies of a content output"""

    Dependencies: list[DependencyState] = field(default_factory=list)
    # The tool could not list the dependencies and the content file includes other files, so the output is always built
    AlwaysBuild: bool = False


def _TryGetFileState(path: str) -> tuple[int, int] | None:
    try:
        stat = os.stat(path)
    except OSError:
        return None
    return (stat.st_size, stat.st_mtime_ns)


def CreateOutputDependencies(contentFileName: str, dependencies: list[str] | None) -> OutputDependencies:
    """The record of a content file that was just built.
    contentFileName: the content file the output was built from, it is tracked by the content cache so it is left out.
    dependencies: the files the tool says the output depends on, None if the tool can not list them.
    """
    if dependencies is None:
        return OutputDependencies(AlwaysBuild=ContainsInclude(contentFileName))

    contentFileId = os.path.normcase(IOUtil.NormalizePath(contentFileName))
    states: dict[str, DependencyState] = {}
    for entry in dependencies:
        path = IOUtil.NormalizePath(entry)
        pathId = os.path.normcase(path)
        if pathId == contentFileId or pathId in states:
            continue
        fileState = _TryGetFileState(path)
        if fileState is None:
            # The tool listed a file that is not there, so there is no state to compare against next time
            return OutputDependencies(AlwaysBuild=True)
        states[pathId] = DependencyState(path, fileState[0], fileState[1], IOUtil.HashFile(path))
    return OutputDependencies(sorted(states.values(), key=lambda entry: entry.Path))


def _ToJson(record: OutputDependencies) -> dict[str, Any]:
    return {
        "AlwaysBuild": record.AlwaysBuild,
        "Dependencies": [
            {"Path": entry.Path, "Length": entry.Length, "ModifiedTime": entry.ModifiedTime, "Checksum": entry.Checksum} for entry in record.Dependencies
        ],
    }


def _FromJson(value: Any) -> OutputDependencies:
    if not isinstance(value, dict) or not isinstance(value.get("AlwaysBuild"), bool) or not isinstance(value.get("Dependencies"), list):
        raise ValueError("invalid output entry")
    dependencies: list[DependencyState] = []
    for entry in value["Dependencies"]:
        if not isinstance(entry, dict):
            raise ValueError("invalid dependency entry")
        path, length, modifiedTime, checksum = entry.get("Path"), entry.get("Length"), entry.get("ModifiedTime"), entry.get("Checksum")
        if not isinstance(path, str) or not isinstance(length, int) or not isinstance(modifiedTime, int) or not isinstance(checksum, str):
            raise ValueError("invalid dependency entry")
        dependencies.append(DependencyState(path, length, modifiedTime, checksum))
    return OutputDependencies(dependencies, value["AlwaysBuild"])


@final
class ContentDependencyCache:
    """Remembers the files each content output depends on beyond its content file (like the files a shader includes), so a change to one of them
    builds the output again. Every output a content processor built is recorded, an output without a record has unknown dependencies and is built.
    """

    def __init__(self, log: Log, cacheFileName: str) -> None:
        super().__init__()
        self.CacheFileName = cacheFileName
        self.__Previous = self.__Load(log, cacheFileName)
        self.__Current: dict[str, OutputDependencies] = {}

    @staticmethod
    def __Load(log: Log, cacheFileName: str) -> dict[str, OutputDependencies]:
        if not os.path.isfile(cacheFileName):
            return {}
        try:
            content = json.loads(IOUtil.ReadFileUTF8(cacheFileName))
            if not isinstance(content, dict) or content.get("Format") != _FORMAT_VERSION or not isinstance(content.get("Outputs"), dict):
                raise ValueError("unknown format")
            return {name: _FromJson(value) for name, value in content["Outputs"].items()}
        except (OSError, UnicodeDecodeError, ValueError) as ex:
            # Like no cache: every output is built again
            log.LogPrintWarning(f"Content dependency cache at '{cacheFileName}' is invalid, ignoring it. {ex}")
            return {}

    def TryGetBuildReason(self, outputName: str) -> str | None:
        """Why the output has to be built because of its dependencies, None if they are unchanged (the output keeps its record)"""
        previous = self.__Previous.get(outputName)
        if previous is None:
            return "its dependencies are unknown"
        if previous.AlwaysBuild:
            return "it includes files the content tool can not list"

        dependencies: list[DependencyState] = []
        for entry in previous.Dependencies:
            fileState = _TryGetFileState(entry.Path)
            if fileState is None:
                return f"'{entry.Path}' is missing"
            if fileState != (entry.Length, entry.ModifiedTime):
                checksum = IOUtil.HashFile(entry.Path)
                if checksum != entry.Checksum:
                    return f"'{entry.Path}' changed"
                entry = DependencyState(entry.Path, fileState[0], fileState[1], checksum)
            dependencies.append(entry)
        self.__Current[outputName] = OutputDependencies(dependencies)
        return None

    def Set(self, outputName: str, record: OutputDependencies) -> None:
        self.__Current[outputName] = record

    def GetAllDependencies(self) -> list[str]:
        """The files the recorded outputs depend on"""
        return sorted({entry.Path for record in self.__Current.values() for entry in record.Dependencies})

    def Save(self, isComplete: bool) -> None:
        """isComplete: every output was examined, so outputs without a new record are gone. Otherwise (after a error) the old records are kept."""
        outputs = self.__Current if isComplete else {**self.__Previous, **self.__Current}
        content = {"Format": _FORMAT_VERSION, "Outputs": {name: _ToJson(outputs[name]) for name in sorted(outputs)}}
        IOUtil.WriteFileUTF8IfChanged(self.CacheFileName, json.dumps(content, indent=2) + "\n")
