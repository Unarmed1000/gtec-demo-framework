#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2025, Mana Battery
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

import fnmatch
import glob
import os
from collections.abc import Sequence
from typing import TYPE_CHECKING, NamedTuple

from FslBuildGen import IOUtil

# Try importing pathspec, a None module is what tells us it is unavailable
try:
    import pathspec
except ImportError:
    pathspec = None

if TYPE_CHECKING:
    from pathspec import Pattern as PathspecPattern


class GitDirResult:
    def __init__(self, ignored: set[str], kept: set[str]) -> None:
        self.Ignored = ignored
        self.Kept = kept


class _FallbackPattern(NamedTuple):
    Pattern: str
    # False for a negated ('!') pattern that re-includes what an earlier pattern ignored
    Ignores: bool


def _ToDirectoryPathspecLines(lines: list[str]) -> list[str]:
    """
    Like git, remove the trailing '/' that limits a pattern to directories, everything we match is a directory.
    The directory name is then matched without a trailing '/', so 'name/**' (everything inside 'name') does not match 'name' itself,
    which is what git check-ignore reports for the directory.
    """
    result: list[str] = []
    for line in lines:
        if line.endswith("/"):
            line = line[:-1]
            # A lone '/' matches nothing in git
            if line in ("", "!"):
                continue
        result.append(line)
    return result


def _TryParsePathspecPatterns(lines: list[str]) -> Sequence[PathspecPattern] | None:
    if pathspec is None:
        return None
    try:
        return pathspec.GitIgnoreSpec.from_lines(_ToDirectoryPathspecLines(lines)).patterns
    except Exception:
        # The fnmatch fallback is used if pathspec fails
        return None


def _IsIgnoredByPathspec(patterns: Sequence[PathspecPattern], directoryName: str) -> bool:
    # Like git the patterns are evaluated in order and the last one that matches decides, so a negated pattern can re-include a directory.
    # A directory is only ignored when a pattern matches the directory itself: 'name/**' ignores what is inside 'name' but keeps 'name'.
    ignored = False
    for pattern in patterns:
        if pattern.include is not None and pattern.match_file(directoryName) is not None:
            ignored = pattern.include
    return ignored


def _TryParseFallbackPattern(line: str) -> _FallbackPattern | None:
    """
    Convert a gitignore line to a fnmatch pattern that is matched against the name of a direct sub directory.
    This is a simplification of the gitignore rules with these known limits:
    - Only a leading '\\' is treated as an escape, a '\\' elsewhere in the pattern is a literal character.
    - fnmatch does not support '[^...]' or the '[[:class:]]' character classes.
    - fnmatch follows the case sensitivity of the platform.
    Like git (and the pathspec matcher) a pattern must match the directory itself, so 'name/**' (everything inside 'name') keeps 'name'.
    """
    pattern = line
    ignores = True
    if pattern.startswith("!"):
        ignores = False
        pattern = pattern[1:]
    if pattern.startswith("\\") and len(pattern) > 1:
        # Escapes the first character, for example '\!' and '\#'
        pattern = glob.escape(pattern[1]) + pattern[2:]
    # A leading '/' anchors the pattern to the directory of the .gitignore file, which is where the direct sub directories are
    pattern = pattern.removeprefix("/")
    # A leading '**/' matches in all directories
    while pattern.startswith("**/") and len(pattern) > 3:
        pattern = pattern[3:]
    # A trailing '/' only limits the pattern to directories and everything we match is a directory
    pattern = pattern.removesuffix("/")
    # A pattern that still contains a '/' is anchored deeper and can not name a direct sub directory.
    # That includes 'name/**' and 'name/**/', they match what is inside 'name' but not 'name' itself.
    if len(pattern) <= 0 or "/" in pattern:
        return None
    return _FallbackPattern(pattern, ignores)


def _IsIgnoredByFallback(patterns: list[_FallbackPattern], directoryName: str) -> bool:
    # Like git the last matching pattern decides
    ignored = False
    for pattern in patterns:
        if fnmatch.fnmatch(directoryName, pattern.Pattern):
            ignored = pattern.Ignores
    return ignored


class GitIgnoreFile:
    @staticmethod
    def TryGetIgnoredDirectories(baseDir: str, gitignoreFile: str = ".gitignore") -> GitDirResult | None:
        """
        Return GitDirResult(ignored, kept) of direct subdirectories of baseDir,
        according to patterns in gitignoreFile. Returns None if the file cannot be opened or parsed.
        Uses pathspec if available, otherwise falls back to simple fnmatch matching.
        This emulates git for the one .gitignore file, it is the fallback when git itself can not be asked (see PackageIgnoredDirectories).
        Like git a directory is only ignored when a pattern matches the directory itself, 'name/**' keeps the directory 'name'.
        """
        try:
            # Like git the file is UTF-8 and a BOM at the start is skipped. Bytes that are not valid UTF-8 (a file saved as ANSI) are kept
            # undecoded, so like in git they only match a name with the same bytes and never a UTF-8 name, the rest of the file still applies.
            with open(gitignoreFile, encoding="utf-8-sig", errors="surrogateescape") as f:
                lines = [line.strip() for line in f if line.strip() and not line.startswith("#")]
        except OSError:
            return None

        ignored: set[str] = set()
        kept: set[str] = set()

        pathspecPatterns = _TryParsePathspecPatterns(lines)
        fallbackPatterns: list[_FallbackPattern] = []
        if pathspecPatterns is None:
            for line in lines:
                fallbackPattern = _TryParseFallbackPattern(line)
                if fallbackPattern is not None:
                    fallbackPatterns.append(fallbackPattern)

        for entry in os.listdir(baseDir):
            fullPath = os.path.join(baseDir, entry)
            if not os.path.isdir(fullPath):
                continue

            if pathspecPatterns is not None:
                isIgnored = _IsIgnoredByPathspec(pathspecPatterns, entry)
            else:
                isIgnored = _IsIgnoredByFallback(fallbackPatterns, entry)
            if isIgnored:
                ignored.add(entry)
            else:
                kept.add(entry)

        return GitDirResult(ignored, kept)

    @staticmethod
    def TryGetDirectories(filename: str) -> GitDirResult | None:
        contentDir = IOUtil.GetDirectoryName(filename)
        return GitIgnoreFile.TryGetIgnoredDirectories(contentDir, filename)
