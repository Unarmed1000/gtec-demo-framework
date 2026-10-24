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

# The files of a content directory that are not content on every platform.
#
# A package has one 'Content' and one 'Content.bld' directory, whatever it is built for. The helper file of the directory ('Content.json',
# which is the command file in 'Content.bld') lists the files that belong to some platforms only:
#
#   { "ContentBuilder": { "Version": "2",
#       "Platforms": [
#         { "Only":   ["Unity"], "Content": ["Materials", "Shaders/*.shader", "*.meta"] },
#         { "Except": ["Unity"], "Content": ["Effects/*.fx"] } ] } }
#
# - A file no entry matches is content on every platform: that is the normal case and needs no helper file.
# - 'Only': content on the named platforms. 'Except': content on every platform but those.
# - An entry of 'Content' is a file, a directory with everything below it, or a pattern with '*' in the file name. A pattern without a
#   directory part ('*.meta') matches in every directory, one with a directory part ('Shaders/*.shader') in that directory.
# - Names are compared with their case: 'Shaders' is not 'shaders'.
# - A platform name the tool does not know, an entry that matches no file and a file two entries disagree about are errors: each of them
#   is a list that does not say what its writer meant.

import fnmatch
from collections.abc import Iterable, Sequence
from typing import Any

from FslBuildGen import IOUtil, PackageConfig
from FslBuildGen.Exceptions import UsageErrorException

RootElement = "ContentBuilder"
VersionElement = "Version"
PlatformsElement = "Platforms"
# The version of the helper file that has the platform lists
VersionWithPlatforms = "2"

_g_only = "Only"
_g_except = "Except"
_g_content = "Content"


class ContentPlatformEntry:
    def __init__(self, description: str, platformNames: frozenset[str], patterns: Sequence[str]) -> None:
        """description: the entry as its writer wrote it, for the messages
        platformNames: the platforms the files of the entry are content on
        """
        super().__init__()
        self.Description = description
        self.PlatformNames = platformNames
        self.Patterns = list(patterns)

    def IsMatch(self, relativePath: str) -> bool:
        return any(IsPatternMatch(pattern, relativePath) for pattern in self.Patterns)


def IsPatternMatch(pattern: str, relativePath: str) -> bool:
    """True if the file is the one the entry names, is below the directory it names or matches its pattern"""
    if "*" not in pattern:
        # A file, or a directory with everything below it
        return relativePath == pattern or relativePath.startswith(pattern + "/")
    fileName = IOUtil.GetFileName(relativePath)
    if "/" not in pattern:
        # In every directory
        return fnmatch.fnmatchcase(fileName, pattern)
    directory = IOUtil.GetDirectoryName(pattern)
    return IOUtil.GetDirectoryName(relativePath) == directory and fnmatch.fnmatchcase(fileName, IOUtil.GetFileName(pattern))


class ContentPlatformList:
    def __init__(self, sourceFilename: str, entries: Sequence[ContentPlatformEntry]) -> None:
        super().__init__()
        self.SourceFilename = sourceFilename
        self.Entries = list(entries)

    def GetFilesNotForPlatform(self, relativePaths: Iterable[str], platformName: str) -> set[str]:
        """The files of the directory that are not content on the platform.
        relativePaths: every file of the directory the list describes, relative to it and with '/'
        """
        matchedEntries: set[int] = set()
        result: set[str] = set()
        for relativePath in relativePaths:
            verdict: ContentPlatformEntry | None = None
            for index, entry in enumerate(self.Entries):
                if not entry.IsMatch(relativePath):
                    continue
                matchedEntries.add(index)
                if verdict is None:
                    verdict = entry
                elif verdict.PlatformNames != entry.PlatformNames:
                    raise UsageErrorException(
                        f"The file '{relativePath}' is listed by two entries of '{PlatformsElement}' in '{self.SourceFilename}' that do not say "
                        f"the same: {verdict.Description} and {entry.Description}. List it in one of them"
                    )
            if verdict is not None and platformName not in verdict.PlatformNames:
                result.add(relativePath)

        for index, entry in enumerate(self.Entries):
            if index not in matchedEntries:
                raise UsageErrorException(
                    f"The entry {entry.Description} of '{PlatformsElement}' in '{self.SourceFilename}' matches no file. Its '{_g_content}' "
                    "holds files, directories and patterns with '*' in the file name, relative to the directory of the file and with the "
                    "case the names have"
                )
        return result

    @staticmethod
    def Parse(jsonPlatforms: Any, sourceFilename: str) -> ContentPlatformList:
        """jsonPlatforms: the value of 'Platforms' in the helper file"""
        if not isinstance(jsonPlatforms, list):
            raise UsageErrorException(f"'{PlatformsElement}' in '{sourceFilename}' is not a list of entries")
        allPlatformNames = frozenset(PackageConfig.APPROVED_PLATFORM_NAMES)
        entries: list[ContentPlatformEntry] = []
        for jsonEntry in jsonPlatforms:
            entries.append(ContentPlatformList.__ParseEntry(jsonEntry, sourceFilename, allPlatformNames))
        return ContentPlatformList(sourceFilename, entries)

    @staticmethod
    def __ParseEntry(jsonEntry: Any, sourceFilename: str, allPlatformNames: frozenset[str]) -> ContentPlatformEntry:
        where = f"'{PlatformsElement}' in '{sourceFilename}'"
        if not isinstance(jsonEntry, dict):
            raise UsageErrorException(f"An entry of {where} is not an object with '{_g_only}' or '{_g_except}', and '{_g_content}'")
        unknownKeys = sorted(str(key) for key in jsonEntry if key not in (_g_only, _g_except, _g_content))
        if len(unknownKeys) > 0:
            raise UsageErrorException(f"An entry of {where} has the unknown key '{unknownKeys[0]}'. Valid keys are: {_g_only}, {_g_except}, {_g_content}")
        if (_g_only in jsonEntry) == (_g_except in jsonEntry):
            raise UsageErrorException(f"An entry of {where} needs one of '{_g_only}' and '{_g_except}', not both and not neither")

        key = _g_only if _g_only in jsonEntry else _g_except
        names = ContentPlatformList.__ParseStringList(jsonEntry[key], key, where)
        for name in names:
            if name not in allPlatformNames:
                raise UsageErrorException(
                    f"'{name}' in '{key}' of {where} is not a platform. Valid platforms are: {', '.join(PackageConfig.APPROVED_PLATFORM_NAMES)}"
                )
        description = f"{{{key}: [{', '.join(names)}]}}"
        if _g_content not in jsonEntry:
            raise UsageErrorException(f"The entry {description} of {where} has no '{_g_content}': the files it is about")
        patterns = ContentPlatformList.__ParseStringList(jsonEntry[_g_content], _g_content, where)
        for pattern in patterns:
            CheckPattern(pattern, f"an entry of {where}")

        platformNames = frozenset(names) if key == _g_only else allPlatformNames - frozenset(names)
        return ContentPlatformEntry(description, platformNames, patterns)

    @staticmethod
    def __ParseStringList(jsonList: Any, key: str, where: str) -> list[str]:
        if not isinstance(jsonList, list) or len(jsonList) <= 0 or not all(isinstance(entry, str) and len(entry) > 0 for entry in jsonList):
            raise UsageErrorException(f"'{key}' of an entry of {where} is not a list of at least one name")
        return [str(entry) for entry in jsonList]


def CheckPattern(pattern: str, where: str) -> None:
    """Stops at an entry that is no path below the directory of the file. where: what holds the entry, for the message"""
    if "\\" in pattern or pattern.startswith("/") or pattern.endswith("/") or ".." in pattern.split("/"):
        raise UsageErrorException(
            f"'{pattern}' in '{_g_content}' of {where} is not valid: it is a path relative to the directory of the file, with '/' between its parts"
        )
    if "*" in IOUtil.GetDirectoryName(pattern):
        raise UsageErrorException(
            f"'{pattern}' in '{_g_content}' of {where} has a '*' in a directory name. A '*' is for the file name; a pattern without a "
            "directory part matches in every directory"
        )
