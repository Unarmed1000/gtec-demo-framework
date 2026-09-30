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

"""
Read the settings that decide how a git repository applies its ignore rules directly from the files in its git directory, no git process
is started. PackageIgnoredDirectories uses them to decide whether the answer of the one 'git check-ignore' in the top-level work tree is
also git's answer for a nested repository (a submodule, a linked work tree or a nested clone): that query applies the .gitignore files of
the nested repository, but neither its info/exclude nor its core.ignorecase.
"""

import os
from typing import NamedTuple

from FslBuildGen import IOUtil
from FslBuildGen.Generator.GitCheckIgnore import TryFindWorkTreeRoot

_TRUE_VALUES = {"true", "yes", "on"}
_FALSE_VALUES = {"false", "no", "off", ""}


class RepositorySettings(NamedTuple):
    # The git directory of the repository, None when it could not be found
    GitDir: str | None
    # True when info/exclude could contain a pattern (a line that is not blank and does not start with '#', or the file can not be read)
    HasActiveExclude: bool
    # The core.ignorecase value in the configuration of the repository, None when it is not set there
    IgnoreCase: bool | None
    # False when the values may be incomplete: the configuration has an include, the repository has a config.worktree, a file can not be read
    Certain: bool


def TryFindTopWorkTreeRoot(workTreeRoot: str, cache: dict[str, str | None]) -> str:
    """Return the outermost work tree that contains the work tree root 'workTreeRoot', or 'workTreeRoot' when it is not inside another one"""
    current = workTreeRoot
    while True:
        parent = IOUtil.NormalizePath(os.path.dirname(current))
        if parent == current:
            return current
        outer = TryFindWorkTreeRoot(parent, cache)
        if outer is None:
            return current
        current = outer


def ReadRepositorySettings(workTreeRoot: str) -> RepositorySettings:
    gitDir = _TryFindGitDir(workTreeRoot)
    if gitDir is None:
        return RepositorySettings(None, True, None, False)
    # A linked work tree keeps its configuration and info/exclude in the common git directory
    commonDir = _TryReadPath(IOUtil.Join(gitDir, "commondir"), gitDir) or gitDir
    hasActiveExclude = _HasActiveExclude(IOUtil.Join(commonDir, "info/exclude"))
    try:
        with open(IOUtil.Join(commonDir, "config"), encoding="utf-8", errors="replace") as file:
            ignoreCase, certain = _ParseIgnoreCase(file.read())
    except OSError:
        return RepositorySettings(gitDir, hasActiveExclude, None, False)
    if os.path.exists(IOUtil.Join(gitDir, "config.worktree")):
        certain = False
    return RepositorySettings(gitDir, hasActiveExclude, ignoreCase, certain)


def _ToAbsolutePath(path: str, relativeTo: str) -> str:
    return IOUtil.NormalizePath(os.path.normpath(path if os.path.isabs(path) else os.path.join(relativeTo, path)))


def _TryReadPath(filename: str, relativeTo: str) -> str | None:
    """The path a file like 'commondir' contains, relative paths are relative to 'relativeTo'"""
    try:
        with open(filename, encoding="utf-8", errors="replace") as file:
            path = file.read().strip()
    except OSError:
        return None
    return _ToAbsolutePath(path, relativeTo) if len(path) > 0 else None


def _TryFindGitDir(workTreeRoot: str) -> str | None:
    gitEntry = IOUtil.Join(workTreeRoot, ".git")
    if os.path.isdir(gitEntry):
        return gitEntry
    try:
        with open(gitEntry, encoding="utf-8", errors="replace") as file:
            lines = file.read().splitlines()
    except OSError:
        return None
    for line in lines:
        if line.startswith("gitdir:"):
            path = line[len("gitdir:") :].strip()
            return _ToAbsolutePath(path, workTreeRoot) if len(path) > 0 else None
    return None


def _HasActiveExclude(filename: str) -> bool:
    try:
        with open(filename, encoding="utf-8", errors="replace") as file:
            lines = file.read().splitlines()
    except FileNotFoundError:
        return False
    except OSError:
        return True
    # Only a line that starts with '#' is a comment, a line with only white space is empty (git removes trailing spaces)
    return any(len(line.strip()) > 0 and not line.startswith("#") for line in lines)


def _ParseBool(value: str) -> bool | None:
    value = value.lower()
    if value in _TRUE_VALUES:
        return True
    if value in _FALSE_VALUES:
        return False
    try:
        return int(value) != 0
    except ValueError:
        return None


def _StripValue(value: str) -> str:
    """Remove a trailing comment and the quotes from a git config value"""
    result: list[str] = []
    quoted = False
    for character in value.strip():
        if character == '"':
            quoted = not quoted
        elif character in ";#" and not quoted:
            break
        else:
            result.append(character)
    return "".join(result).strip()


def _ParseIgnoreCase(text: str) -> tuple[bool | None, bool]:
    """Return the last core.ignorecase value of a git config file (None when it is not set) and whether that value is certain"""
    section = ""
    ignoreCase: bool | None = None
    for rawLine in text.splitlines():
        line = rawLine.strip()
        if line.endswith("\\"):
            # A value that continues on the next line is not parsed
            return ignoreCase, False
        if len(line) <= 0 or line[0] in "#;":
            continue
        if line.startswith("["):
            end = line.find("]")
            if end < 0:
                return ignoreCase, False
            header = line[1:end].split(None, 1)
            section = header[0].lower() if len(header) > 0 else ""
            if section in ("include", "includeif"):
                # An included file could set core.ignorecase
                return ignoreCase, False
            line = line[end + 1 :].strip()
            if len(line) <= 0:
                continue
        if section == "core":
            key, separator, value = line.partition("=")
            if _StripValue(key).lower() == "ignorecase":
                # A key without a value is true
                parsedValue = _ParseBool(_StripValue(value)) if len(separator) > 0 else True
                if parsedValue is None:
                    return ignoreCase, False
                ignoreCase = parsedValue
    return ignoreCase, True
