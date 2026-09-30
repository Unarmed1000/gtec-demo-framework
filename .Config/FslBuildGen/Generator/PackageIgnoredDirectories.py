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

One git process answers for all packages below a top-level work tree, also for the packages in its submodules and other nested
repositories: with --no-index git applies every .gitignore on the path, including the ones of a nested repository. That answer is git's
answer for a nested repository when the pattern that decided is in a .gitignore of that repository (or nothing matched), the repository
has no info/exclude patterns and uses the same core.ignorecase. Otherwise the package is asked in its own work tree.
"""

import os
from collections.abc import Iterable
from concurrent.futures import ThreadPoolExecutor
from enum import Enum
from typing import NamedTuple

from FslBuildGen import IOUtil
from FslBuildGen.Generator.GitCheckIgnore import GitCheckIgnoreError, GitCheckIgnoreRecord, RunCheckIgnore, TryFindWorkTreeRoot
from FslBuildGen.Generator.GitIgnoreFile import GitDirResult, GitIgnoreFile
from FslBuildGen.Generator.GitRepositorySettings import ReadRepositorySettings, RepositorySettings, TryFindTopWorkTreeRoot
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

    def __init__(self, packagePath: str, root: str, topRoot: str, subDirectories: list[str]) -> None:
        super().__init__()
        self.PackagePath = packagePath
        # The work tree that contains the package, and the outermost work tree that contains that one (the same when it is not nested)
        self.Root = root
        self.TopRoot = topRoot
        self.SubDirectories = subDirectories

    def GetRelativePath(self, fromTop: bool) -> str:
        """The package directory relative to the work tree (or the top-level work tree), empty when the package is that root"""
        return _GetRelativePath(self.TopRoot if fromTop else self.Root, self.PackagePath)

    def GetRelativePathOf(self, subDirectory: str, fromTop: bool) -> str:
        relativePath = self.GetRelativePath(fromTop)
        return f"{relativePath}/{subDirectory}" if len(relativePath) > 0 else subDirectory

    def GetAllRelativePaths(self, fromTop: bool) -> list[str]:
        # The root itself is never ignored and git can not check an empty path
        relativePath = self.GetRelativePath(fromTop)
        paths = [relativePath] if len(relativePath) > 0 else []
        paths += [self.GetRelativePathOf(subDirectory, fromTop) for subDirectory in self.SubDirectories]
        return paths

    def GetRecords(self, answer: dict[str, GitCheckIgnoreRecord], fromTop: bool) -> tuple[GitCheckIgnoreRecord | None, dict[str, GitCheckIgnoreRecord]]:
        """The record of the package directory (None when the package is the root that was asked) and the record of each sub directory"""
        relativePath = self.GetRelativePath(fromTop)
        packageRecord = answer[relativePath] if len(relativePath) > 0 else None
        return packageRecord, {subDirectory: answer[self.GetRelativePathOf(subDirectory, fromTop)] for subDirectory in self.SubDirectories}


class _QueryFailure(NamedTuple):
    Message: str
    # git is not installed, asking another work tree can not help
    ExecutableNotFound: bool


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


def _IsDecidedInside(record: GitCheckIgnoreRecord, topRoot: str, root: str) -> bool:
    """
    True if nothing matched the path, or the pattern that decided is in a .gitignore inside the work tree 'root' (for an answer of the
    top-level work tree 'topRoot'). git searches the deepest .gitignore first and info/exclude last, so asking 'root' finds the same pattern.
    """
    if len(record.Pattern) <= 0:
        return True
    if os.path.basename(record.Source) != _GITIGNORE_FILENAME:
        return False
    sourcePath = IOUtil.NormalizePath(record.Source if os.path.isabs(record.Source) else IOUtil.Join(topRoot, record.Source))
    return sourcePath.startswith(root if root.endswith("/") else root + "/")


class PackageIgnoredDirectories:
    def __init__(self, log: Log, runner: GitRunner | None, packagePaths: Iterable[str], maxParallelQueries: int = 8) -> None:
        """
        log: receives the verbose output and the warning when git fails.
        runner: runs git, None never runs git so the emulation decides (used by the tests).
        packagePaths: the packages git is asked about together on the first Resolve, one git process per top-level work tree.
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
        self.__RepositorySettings: dict[str, RepositorySettings] = {}
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
        gitQueryCountBefore = self.GitQueryCount

        # The reason git does not decide for a package that has a .gitignore
        fallbackReasons: dict[str, str] = {}
        topQueries: dict[str, list[_PackageQuery]] = {}
        # The packages that are asked in their own (nested) work tree, and why that work tree is asked
        ownQueries: dict[str, list[_PackageQuery]] = {}
        ownQueryReasons: dict[str, str] = {}

        def AddOwnQuery(query: _PackageQuery, reason: str) -> None:
            ownQueries.setdefault(query.Root, []).append(query)
            ownQueryReasons.setdefault(query.Root, reason)

        for packagePath in pending:
            if not os.path.isfile(IOUtil.Join(packagePath, _GITIGNORE_FILENAME)):
                self.__Results[packagePath] = PackageIgnoredDirectoriesResult(None, ExcludeDirectorySource.Legacy, "the package has no .gitignore", {})
                continue
            queryOrReason = self.__TryCreateQuery(packagePath)
            if isinstance(queryOrReason, str):
                fallbackReasons[packagePath] = queryOrReason
                continue
            ownQueryReason = self.__TryGetOwnQueryReason(queryOrReason)
            if ownQueryReason is not None:
                AddOwnQuery(queryOrReason, ownQueryReason)
            else:
                topQueries.setdefault(queryOrReason.TopRoot, []).append(queryOrReason)

        failures: list[str] = []
        topAnswers = self.__RunQueries(
            {topRoot: [path for query in queries for path in query.GetAllRelativePaths(True)] for topRoot, queries in topQueries.items()}
        )
        for topRoot, queries in topQueries.items():
            answer = topAnswers[topRoot]
            if isinstance(answer, _QueryFailure):
                # The packages of a nested repository can still be answered by their own work tree (unless git is not installed)
                for query in queries:
                    if query.Root == topRoot or answer.ExecutableNotFound:
                        fallbackReasons[query.PackagePath] = f"git failed: {answer.Message}"
                    else:
                        AddOwnQuery(query, f"the query in the top-level work tree failed: {answer.Message}")
                if answer.ExecutableNotFound or any(query.Root == topRoot for query in queries):
                    failures.append(answer.Message)
                continue
            for query in queries:
                packageRecord, subRecords = query.GetRecords(answer, True)
                if query.Root != topRoot:
                    records = [*([packageRecord] if packageRecord is not None else []), *subRecords.values()]
                    outsideRecord = next((record for record in records if not _IsDecidedInside(record, topRoot, query.Root)), None)
                    if outsideRecord is not None:
                        # A rule of the top-level work tree does not apply inside the nested repository
                        AddOwnQuery(query, f"a rule outside it matched ({outsideRecord.Describe()})")
                        continue
                self.__StoreGitResult(query, packageRecord, subRecords, fallbackReasons)

        for root, reason in ownQueryReasons.items():
            self.__Log.LogPrintVerbose(2, f"git is asked in '{root}' on its own: {reason}")
        ownAnswers = self.__RunQueries({root: [path for query in queries for path in query.GetAllRelativePaths(False)] for root, queries in ownQueries.items()})
        for root, queries in ownQueries.items():
            answer = ownAnswers[root]
            if isinstance(answer, _QueryFailure):
                failures.append(answer.Message)
                for query in queries:
                    fallbackReasons[query.PackagePath] = f"git failed: {answer.Message}"
                continue
            for query in queries:
                packageRecord, subRecords = query.GetRecords(answer, False)
                self.__StoreGitResult(query, packageRecord, subRecords, fallbackReasons)

        for packagePath, reason in fallbackReasons.items():
            emulated = GitIgnoreFile.TryGetDirectories(IOUtil.Join(packagePath, _GITIGNORE_FILENAME))
            if emulated is not None:
                self.__Results[packagePath] = PackageIgnoredDirectoriesResult(emulated, ExcludeDirectorySource.Emulation, reason, {})
            else:
                self.__Results[packagePath] = PackageIgnoredDirectoriesResult(
                    None, ExcludeDirectorySource.Legacy, f"{reason}, and the package .gitignore could not be read", {}
                )

        self.__ReportFailures(failures)
        self.__LogSummary(pending, self.GitQueryCount - gitQueryCountBefore, len(ownQueries))

    def __TryCreateQuery(self, packagePath: str) -> _PackageQuery | str:
        """Return the query for the package, or the reason why git is not asked"""
        if self.__Runner is None:
            return "git is disabled"
        root = TryFindWorkTreeRoot(packagePath, self.__WorkTreeRootCache)
        if root is None:
            return "the package is not in a git work tree"
        topRoot = TryFindTopWorkTreeRoot(root, self.__WorkTreeRootCache)
        # git stops with a fatal error for a path beyond a symbolic link, and the package would not be where the path says
        if topRoot != root and _ContainsLink(topRoot, _GetRelativePath(topRoot, packagePath)):
            # The link can be on the way to the nested repository, then that repository is asked like a top-level one
            topRoot = root
        if _ContainsLink(topRoot, _GetRelativePath(topRoot, packagePath)):
            return f"the path from the work tree root '{topRoot}' to the package contains a symbolic link or junction"
        return _PackageQuery(packagePath, root, topRoot, _GetSubDirectories(packagePath))

    def __GetRepositorySettings(self, root: str) -> RepositorySettings:
        settings = self.__RepositorySettings.get(root)
        if settings is None:
            settings = ReadRepositorySettings(root)
            self.__RepositorySettings[root] = settings
        return settings

    def __TryGetOwnQueryReason(self, query: _PackageQuery) -> str | None:
        """Return why the query of the top-level work tree can not answer for the nested repository of the package, None when it can"""
        if query.Root == query.TopRoot:
            return None
        settings = self.__GetRepositorySettings(query.Root)
        if settings.GitDir is None:
            return "its git directory was not found"
        if settings.HasActiveExclude:
            return "its info/exclude has patterns"
        topSettings = self.__GetRepositorySettings(query.TopRoot)
        if not (settings.Certain and topSettings.Certain and settings.IgnoreCase == topSettings.IgnoreCase):
            return "its core.ignorecase can differ from the one of the top-level work tree"
        return None

    def __RunQueries(self, pathsByRoot: dict[str, list[str]]) -> dict[str, dict[str, GitCheckIgnoreRecord] | _QueryFailure]:
        """Run one git check-ignore per root, in parallel"""
        runner = self.__Runner
        if runner is None or len(pathsByRoot) <= 0:
            return {}

        def RunQuery(root: str, paths: list[str]) -> dict[str, GitCheckIgnoreRecord] | _QueryFailure:
            try:
                # A sub package directory is also a sub directory of its parent package, it is only sent once
                return RunCheckIgnore(runner, root, list(dict.fromkeys(paths)))
            except GitRunError as ex:
                return _QueryFailure(str(ex), ex.ExecutableNotFound)
            except GitCheckIgnoreError as ex:
                return _QueryFailure(str(ex), False)

        self.GitQueryCount += len(pathsByRoot)
        maxWorkers = min(self.__MaxParallelQueries, len(pathsByRoot))
        with ThreadPoolExecutor(max_workers=maxWorkers) as executor:
            futures = {root: executor.submit(RunQuery, root, paths) for root, paths in pathsByRoot.items()}
            return {root: future.result() for root, future in futures.items()}

    def __StoreGitResult(
        self,
        query: _PackageQuery,
        packageRecord: GitCheckIgnoreRecord | None,
        subRecords: dict[str, GitCheckIgnoreRecord],
        fallbackReasons: dict[str, str],
    ) -> None:
        """Store the git answer for the package, or the reason why it can not be used in 'fallbackReasons'"""
        # Everything below an ignored directory is ignored, git would exclude every sub directory
        if packageRecord is not None and packageRecord.IsIgnored:
            fallbackReasons[query.PackagePath] = f"the package directory is ignored by git ({packageRecord.Describe()})"
            return
        ignored: set[str] = set()
        kept: set[str] = set()
        details: dict[str, GitCheckIgnoreRecord] = {}
        for subDirectory, record in subRecords.items():
            if record.IsIgnored:
                ignored.add(subDirectory)
                details[subDirectory] = record
            else:
                kept.add(subDirectory)
        self.__Results[query.PackagePath] = PackageIgnoredDirectoriesResult(GitDirResult(ignored, kept), ExcludeDirectorySource.Git, None, details)

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

    def __LogSummary(self, packagePaths: list[str], queryCount: int, ownQueryCount: int) -> None:
        if len(packagePaths) <= 0:
            return
        counts = dict.fromkeys(ExcludeDirectorySource, 0)
        for packagePath in packagePaths:
            counts[self.__Results[packagePath].Source] += 1
        countText = ", ".join(f"{source.value}: {count}" for source, count in counts.items())
        ownText = f" ({ownQueryCount} for nested repositories asked on their own)" if ownQueryCount > 0 else ""
        self.__Log.LogPrintVerbose(1, f"Ignored package directories of {len(packagePaths)} packages ({countText}) using {queryCount} git queries{ownText}")
