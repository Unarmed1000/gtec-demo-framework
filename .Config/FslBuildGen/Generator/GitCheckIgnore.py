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
Ask 'git check-ignore' which paths of a work tree are ignored.

The command is 'git --no-optional-locks -c core.excludesFile=<os.devnull> -C <root> check-ignore --no-index --verbose --non-matching -z --stdin':
- '--no-index' evaluates the ignore rules only, without it a directory that contains a tracked file is never reported as ignored.
- 'core.excludesFile=<os.devnull>' switches off the personal global ignore file of the developer (by default ~/.config/git/ignore).
  The work tree's own rules (the .gitignore files from the root down and .git/info/exclude) and its 'core.ignorecase' still apply.
- '--verbose --non-matching -z' prints 'source NUL line NUL pattern NUL path NUL' for every path, the first three fields are empty
  when no pattern matched. A negated ('!') pattern that matched is also printed, so a path is only ignored when the pattern does not
  start with '!'. The exit code is 0 when a pattern matched (negated or not) and 1 when nothing matched, both are a success.
  Exit code 128 is fatal (not a repository, dubious ownership, a path outside the repository or beyond a symbolic link) and one bad
  path aborts the whole batch.
"""

import os
from collections.abc import Sequence
from typing import NamedTuple

from FslBuildGen import IOUtil
from FslBuildGen.GitRunner import GitRunner

_GIT_ENTRY_NAME = ".git"


class GitCheckIgnoreRecord(NamedTuple):
    # The file that contains the pattern that matched, relative to the root for a .gitignore. Empty when nothing matched.
    Source: str
    # The line of the pattern in Source, None when nothing matched
    LineNumber: int | None
    # The pattern as written in Source (a negated pattern keeps its '!'). Empty when nothing matched.
    Pattern: str
    # The path exactly as it was sent to git
    Path: bytes

    @property
    def IsIgnored(self) -> bool:
        return len(self.Pattern) > 0 and not self.Pattern.startswith("!")

    def Describe(self) -> str:
        return f"{self.Source}:{self.LineNumber}:{self.Pattern}" if len(self.Pattern) > 0 else "no pattern matched"


class GitCheckIgnoreError(Exception):
    """git check-ignore ran but failed, or its output could not be understood"""


def TryFindWorkTreeRoot(directory: str, cache: dict[str, str | None]) -> str | None:
    """
    Return the root of the git work tree that contains 'directory' (the nearest parent directory, or directory itself, with a '.git'), or None.
    A '.git' file (a submodule or a linked work tree) is a root, a '.git' directory only when it contains a HEAD (git ignores it otherwise).
    No git process is started. Every directory that was visited is added to 'cache'.
    """
    visited: list[str] = []
    current = IOUtil.NormalizePath(directory)
    result: str | None = None
    while True:
        if current in cache:
            result = cache[current]
            break
        visited.append(current)
        gitEntry = IOUtil.Join(current, _GIT_ENTRY_NAME)
        if os.path.isfile(gitEntry) or (os.path.isdir(gitEntry) and os.path.exists(IOUtil.Join(gitEntry, "HEAD"))):
            result = current
            break
        parent = IOUtil.NormalizePath(os.path.dirname(current))
        if parent == current:
            break
        current = parent
    for visitedDirectory in visited:
        cache[visitedDirectory] = result
    return result


def ToCheckIgnorePath(relativePath: str) -> bytes:
    """Encode a root relative path with '/' separators the way git check-ignore reads it from stdin"""
    if len(relativePath) <= 0:
        # git check-ignore stops with a fatal error for an empty path, which would lose the answers for all other paths
        raise ValueError("An empty path can not be checked, the root of the work tree itself is never ignored")
    if "\0" in relativePath:
        raise ValueError(f"The path '{relativePath}' contains a NUL character")
    # A leading ':' starts pathspec magic, './' makes git read it as a plain path
    if relativePath.startswith(":"):
        relativePath = "./" + relativePath
    return os.fsencode(relativePath)


def ParseCheckIgnoreOutput(stdout: bytes, sentPaths: Sequence[bytes]) -> list[GitCheckIgnoreRecord] | None:
    """
    Parse the output of 'git check-ignore --verbose --non-matching -z' for the paths it was given, in the same order.
    Returns None unless there is exactly one well formed record for each sent path.
    """
    if len(stdout) <= 0:
        return [] if len(sentPaths) <= 0 else None
    # Every field ends with a NUL
    if not stdout.endswith(b"\0"):
        return None
    fields = stdout[:-1].split(b"\0")
    if len(fields) != 4 * len(sentPaths):
        return None
    records: list[GitCheckIgnoreRecord] = []
    for index, sentPath in enumerate(sentPaths):
        source, line, pattern, path = fields[index * 4 : index * 4 + 4]
        if path != sentPath:
            return None
        if len(pattern) <= 0:
            if len(source) > 0 or len(line) > 0:
                return None
            records.append(GitCheckIgnoreRecord("", None, "", path))
            continue
        if len(source) <= 0 or not line.isdigit():
            return None
        records.append(GitCheckIgnoreRecord(os.fsdecode(source), int(line), os.fsdecode(pattern), path))
    return records


def RunCheckIgnore(runner: GitRunner, root: str, relativePaths: Sequence[str]) -> dict[str, GitCheckIgnoreRecord]:
    """
    Ask git which of 'relativePaths' (relative to the work tree 'root', '/' separated, a directory without a trailing '/') are ignored.
    All paths are checked by one git process. Returns the record of each path.
    Raises GitCheckIgnoreError if git fails or its output can not be parsed, and GitRunError if git can not be run.
    """
    if len(relativePaths) <= 0:
        return {}
    sentPaths = [ToCheckIgnorePath(relativePath) for relativePath in relativePaths]
    # git reads all of stdin before it writes the answers, subprocess.run writes it all, closes it and then reads the output
    stdinData = b"".join(sentPath + b"\0" for sentPath in sentPaths)
    result = runner.Run(
        root,
        ["check-ignore", "--no-index", "--verbose", "--non-matching", "-z", "--stdin"],
        stdinData=stdinData,
        configOverrides=[("core.excludesFile", os.devnull)],
    )
    if result.ExitCode not in (0, 1):
        raise GitCheckIgnoreError(f"'git check-ignore' failed in '{root}' with exit code {result.ExitCode}: {result.Stderr.strip()}")
    records = ParseCheckIgnoreOutput(result.Stdout, sentPaths)
    if records is None:
        raise GitCheckIgnoreError(f"The output of 'git check-ignore' in '{root}' could not be parsed")
    return dict(zip(relativePaths, records, strict=True))
