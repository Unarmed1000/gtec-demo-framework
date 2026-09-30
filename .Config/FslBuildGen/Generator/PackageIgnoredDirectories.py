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
Decide which direct sub directories of a package are ignored, so the C# project does not compile what they contain.

Git decides when it can: all the rules git sees in the work tree apply (the package .gitignore, the .gitignore files of its parent
directories up to the root, .git/info/exclude and the work tree's core.ignorecase), but not the developer's personal global ignore file.
A sub directory is ignored when git ignores the directory itself, so 'name/**' (the content only) keeps the directory.
The .gitignore emulation (GitIgnoreFile) that uses the same directory rule is the fallback.
"""

import os
from collections.abc import Iterable
from concurrent.futures import ThreadPoolExecutor
from enum import Enum
from typing import NamedTuple

from FslBuildGen import IOUtil
from FslBuildGen.Generator.GitCheckIgnore import GitCheckIgnoreError, GitCheckIgnoreRecord, RunCheckIgnore, TryFindWorkTreeRoot
from FslBuildGen.Generator.GitIgnoreFile import GitDirResult, GitIgnoreFile
from FslBuildGen.GitRunner import GitRunError, GitRunner
from FslBuildGen.Log import Log

_GITIGNORE_FILENAME = ".gitignore"


class ExcludeDirectorySource(Enum):
    # git check-ignore decided
    Git = "git"
    # The .gitignore emulation (GitIgnoreFile) decided
    Emulation = "emulation"
    # There is no usable package .gitignore, the caller must use its legacy directory scan
    Legacy = "legacy"


class PackageIgnoredDirectoriesResult(NamedTuple):
    # The ignored and kept direct sub directories, None when the Source is Legacy
    Result: GitDirResult | None
    Source: ExcludeDirectorySource
    # Why git did not decide, None when it did
    Reason: str | None
    # The pattern git matched for each ignored sub directory, empty unless the Source is Git
    Details: dict[str, GitCheckIgnoreRecord]


class _PackageQuery:
    """The paths of one package that are sent to git"""

    def __init__(self, packagePath: str, root: str, relativePath: str, subDirectories: list[str]) -> None:
        super().__init__()
        self.PackagePath = packagePath
        self.Root = root
        # The package directory relative to the root, empty when the package is the root
        self.RelativePath = relativePath
        self.SubDirectories = subDirectories

    def GetRelativePathOf(self, subDirectory: str) -> str:
        return f"{self.RelativePath}/{subDirectory}" if len(self.RelativePath) > 0 else subDirectory

    def GetAllRelativePaths(self) -> list[str]:
        # The root itself is never ignored and git can not check an empty path
        paths = [self.RelativePath] if len(self.RelativePath) > 0 else []
        paths += [self.GetRelativePathOf(subDirectory) for subDirectory in self.SubDirectories]
        return paths


class _QueryFailure(NamedTuple):
    Message: str


def _GetRelativePath(root: str, path: str) -> str:
    if path == root:
        return ""
    # A drive or file system root already ends with a '/'
    prefix = root if root.endswith("/") else root + "/"
    if not path.startswith(prefix):
        raise ValueError(f"'{path}' is not inside '{root}'")
    return path[len(prefix) :]


def _ContainsLink(root: str, relativePath: str) -> bool:
    """True if a directory on the path from 'root' (excluded) to 'relativePath' (included) is a symbolic link or a junction"""
    if len(relativePath) <= 0:
        return False
    current = root
    for part in relativePath.split("/"):
        current = IOUtil.Join(current, part)
        if os.path.islink(current) or os.path.isjunction(current):
            return True
    return False


def _GetSubDirectories(path: str) -> list[str]:
    # Like GitIgnoreFile a symbolic link to a directory counts as a sub directory, git is asked about its name only
    with os.scandir(path) as entries:
        return sorted(entry.name for entry in entries if entry.is_dir())


class PackageIgnoredDirectories:
    def __init__(self, log: Log, runner: GitRunner | None, packagePaths: Iterable[str], maxParallelQueries: int = 8) -> None:
        """
        log: receives the verbose output and the warning when git fails.
        runner: runs git, None never runs git so the emulation decides (used by the tests).
        packagePaths: the packages git is asked about together on the first Resolve, one git process per work tree.
        """
        super().__init__()
        self.__Log = log
        self.__Runner = runner
        self.__MaxParallelQueries = max(1, maxParallelQueries)
        self.__Pending: list[str] = []
        self.__PendingSet: set[str] = set()
        self.__Results: dict[str, PackageIgnoredDirectoriesResult] = {}
        self.__Logged: set[str] = set()
        self.__WorkTreeRootCache: dict[str, str | None] = {}
        self.__WarningShown = False
        # The number of git processes that were started
        self.GitQueryCount = 0
        for packagePath in packagePaths:
            self.__Register(IOUtil.NormalizePath(packagePath))

    def Resolve(self, packagePath: str) -> PackageIgnoredDirectoriesResult:
        """Return the ignored and kept direct sub directories of the package, and where the answer came from"""
        path = IOUtil.NormalizePath(packagePath)
        if path not in self.__Results:
            # A package that was not given to the constructor is resolved on its own
            self.__Register(path)
            self.__ResolvePending()
        result = self.__Results[path]
        if path not in self.__Logged:
            self.__Logged.add(path)
            self.__LogResult(path, result)
        return result

    def __Register(self, packagePath: str) -> None:
        if packagePath not in self.__PendingSet and packagePath not in self.__Results:
            self.__PendingSet.add(packagePath)
            self.__Pending.append(packagePath)

    def __LogResult(self, packagePath: str, result: PackageIgnoredDirectoriesResult) -> None:
        if result.Reason is None:
            self.__Log.LogPrintVerbose(2, f"Ignored directories of '{packagePath}' come from {result.Source.value}")
        else:
            self.__Log.LogPrintVerbose(2, f"Ignored directories of '{packagePath}' come from {result.Source.value}: {result.Reason}")
        for name in sorted(result.Details.keys()):
            self.__Log.LogPrintVerbose(4, f"  '{name}' is ignored by {result.Details[name].Describe()}")

    def __ResolvePending(self) -> None:
        pending = self.__Pending
        self.__Pending = []
        self.__PendingSet = set()

        # The reason git does not decide for a package that has a .gitignore
        fallbackReasons: dict[str, str] = {}
        queriesByRoot: dict[str, list[_PackageQuery]] = {}
        for packagePath in pending:
            if not os.path.isfile(IOUtil.Join(packagePath, _GITIGNORE_FILENAME)):
                self.__Results[packagePath] = PackageIgnoredDirectoriesResult(None, ExcludeDirectorySource.Legacy, "the package has no .gitignore", {})
                continue
            queryOrReason = self.__TryCreateQuery(packagePath)
            if isinstance(queryOrReason, str):
                fallbackReasons[packagePath] = queryOrReason
            else:
                queriesByRoot.setdefault(queryOrReason.Root, []).append(queryOrReason)

        answers = self.__RunQueries(queriesByRoot)
        failures: list[str] = []
        for root, queries in queriesByRoot.items():
            answer = answers[root]
            if isinstance(answer, _QueryFailure):
                failures.append(answer.Message)
                for query in queries:
                    fallbackReasons[query.PackagePath] = f"git failed: {answer.Message}"
                continue
            for query in queries:
                reason = self.__TryStoreGitResult(query, answer)
                if reason is not None:
                    fallbackReasons[query.PackagePath] = reason

        for packagePath, reason in fallbackReasons.items():
            emulated = GitIgnoreFile.TryGetDirectories(IOUtil.Join(packagePath, _GITIGNORE_FILENAME))
            if emulated is not None:
                self.__Results[packagePath] = PackageIgnoredDirectoriesResult(emulated, ExcludeDirectorySource.Emulation, reason, {})
            else:
                self.__Results[packagePath] = PackageIgnoredDirectoriesResult(
                    None, ExcludeDirectorySource.Legacy, f"{reason}, and the package .gitignore could not be read", {}
                )

        self.__ReportFailures(failures)
        self.__LogSummary(pending, len(queriesByRoot))

    def __TryCreateQuery(self, packagePath: str) -> _PackageQuery | str:
        """Return the query for the package, or the reason why git is not asked"""
        if self.__Runner is None:
            return "git is disabled"
        root = TryFindWorkTreeRoot(packagePath, self.__WorkTreeRootCache)
        if root is None:
            return "the package is not in a git work tree"
        relativePath = _GetRelativePath(root, packagePath)
        # git stops with a fatal error for a path beyond a symbolic link, and the package would not be where the path says
        if _ContainsLink(root, relativePath):
            return f"the path from the work tree root '{root}' to the package contains a symbolic link or junction"
        return _PackageQuery(packagePath, root, relativePath, _GetSubDirectories(packagePath))

    def __RunQueries(self, queriesByRoot: dict[str, list[_PackageQuery]]) -> dict[str, dict[str, GitCheckIgnoreRecord] | _QueryFailure]:
        runner = self.__Runner
        if runner is None or len(queriesByRoot) <= 0:
            return {}

        def RunQuery(root: str, queries: list[_PackageQuery]) -> dict[str, GitCheckIgnoreRecord] | _QueryFailure:
            # A sub package directory is also a sub directory of its parent package, it is only sent once
            paths = list(dict.fromkeys(path for query in queries for path in query.GetAllRelativePaths()))
            try:
                return RunCheckIgnore(runner, root, paths)
            except (GitRunError, GitCheckIgnoreError) as ex:
                return _QueryFailure(str(ex))

        self.GitQueryCount += len(queriesByRoot)
        maxWorkers = min(self.__MaxParallelQueries, len(queriesByRoot))
        with ThreadPoolExecutor(max_workers=maxWorkers) as executor:
            futures = {root: executor.submit(RunQuery, root, queries) for root, queries in queriesByRoot.items()}
            return {root: future.result() for root, future in futures.items()}

    def __TryStoreGitResult(self, query: _PackageQuery, answer: dict[str, GitCheckIgnoreRecord]) -> str | None:
        """Store the git answer for the package, or return the reason why it can not be used"""
        if len(query.RelativePath) > 0:
            packageRecord = answer[query.RelativePath]
            # Everything below an ignored directory is ignored, git would exclude every sub directory
            if packageRecord.IsIgnored:
                return f"the package directory is ignored by git ({packageRecord.Describe()})"
        ignored: set[str] = set()
        kept: set[str] = set()
        details: dict[str, GitCheckIgnoreRecord] = {}
        for subDirectory in query.SubDirectories:
            record = answer[query.GetRelativePathOf(subDirectory)]
            if record.IsIgnored:
                ignored.add(subDirectory)
                details[subDirectory] = record
            else:
                kept.add(subDirectory)
        self.__Results[query.PackagePath] = PackageIgnoredDirectoriesResult(GitDirResult(ignored, kept), ExcludeDirectorySource.Git, None, details)
        return None

    def __ReportFailures(self, failures: list[str]) -> None:
        if len(failures) <= 0:
            return
        for failure in failures:
            self.__Log.LogPrintVerbose(2, f"git check-ignore failed: {failure}")
        if not self.__WarningShown:
            self.__WarningShown = True
            moreText = f" (git also failed for {len(failures) - 1} more work trees)" if len(failures) > 1 else ""
            self.__Log.DoPrintWarning(
                "git could not tell which directories of the C# packages are ignored, "
                + f"the package .gitignore files are matched by the built-in emulation instead: {failures[0]}{moreText}"
            )

    def __LogSummary(self, packagePaths: list[str], queryCount: int) -> None:
        if len(packagePaths) <= 0:
            return
        counts = dict.fromkeys(ExcludeDirectorySource, 0)
        for packagePath in packagePaths:
            counts[self.__Results[packagePath].Source] += 1
        countText = ", ".join(f"{source.value}: {count}" for source, count in counts.items())
        self.__Log.LogPrintVerbose(1, f"Ignored package directories of {len(packagePaths)} packages ({countText}) using {queryCount} git queries")
