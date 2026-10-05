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

# The outcome of a build (Build/Builder.py): what is known about each package after the build and after the '--ForAllExe' / '--ForAll' command
# that was run for it. Without 'FslBuild --KeepGoing' a failure stops the build, so the outcome of a build that returned holds no failure.
# With it the failures are recorded here, the caller prints FormatSummary and decides the exit code (FailureExitCode).
#
# What 'built' means depends on how the generator builds:
# - One package at a time (the legacy generators): a package is built when its build command returned 0, and so are the packages it depends
#   on that have no build command of their own in the run.
# - All packages in one build (the cmake generators, the 'master build'): when the build returned 0 every package is built. When it failed
#   the tool finds out for each package with an executable:
#   - no executable: not built.
#   - an executable that build wrote (it did not exist before the build, or it is not the file it was): built.
#   - an executable an earlier build left: only building that one package tells. It is built when that returns 0 (it was up to date) and
#     not built with the exit code of that build otherwise. A generator that can not build one package leaves it not verified, which
#     counts as failed.
#   A package that is built makes the packages it depends on built too. The other packages without an executable stay unknown.

import os
from collections.abc import Sequence
from enum import Enum

from FslBuildGen.Build import BuildDryRun
from FslBuildGen.DataTypes import PackageType
from FslBuildGen.Packages.Package import Package

# The exit code of a run with '--KeepGoing' where something failed
FailureExitCode = 1


def _Count(count: int, noun: str) -> str:
    return f"{count} {noun}" if count == 1 else f"{count} {noun}s"


class PackageBuildState(Enum):
    # The tool can not tell: nothing was built for the package (a dry run), or the build of all packages failed and the package has no
    # executable to look for
    Unknown = 0
    Built = 1
    NotBuilt = 2
    # The build of all packages failed, the executable of the package is the one an earlier build left and the generator has no way to
    # build the package on its own: it may be up to date or not. Reported as 'not known' and counted as failed.
    NotVerified = 3


class CommandState(Enum):
    # No command was asked for, or it was not run because the package was not built
    NotRun = 0
    # The command returned 0
    Succeeded = 1
    # The command returned something else, see PackageOutcome.CommandExitCode
    Failed = 2
    CouldNotStart = 3
    # The command did not return within the time it was given (ForAllConfig.TimeoutSeconds) and was stopped
    TimedOut = 4


class PackageOutcome:
    def __init__(self, package: Package) -> None:
        super().__init__()
        self.Package = package
        self.Name = package.Name
        self.State = PackageBuildState.Unknown
        # The exit code of the build command of this package, None when the package has no build command of its own
        self.BuildExitCode: int | None = None
        self.CommandState = CommandState.NotRun
        # The exit code of the command, None when it was not run or could not be started
        self.CommandExitCode: int | None = None
        # The command that was run or could not be started
        self.Command: list[str] = []

    @property
    def HasBuildFailed(self) -> bool:
        """True for a package that is not built or not verified: no command is run for it"""
        return self.State == PackageBuildState.NotBuilt or self.State == PackageBuildState.NotVerified

    @property
    def HasFailed(self) -> bool:
        return self.HasBuildFailed or self.CommandState in (CommandState.Failed, CommandState.CouldNotStart, CommandState.TimedOut)

    def DescribeFailure(self) -> str:
        if self.State == PackageBuildState.NotVerified:
            return "not known"
        if self.State == PackageBuildState.NotBuilt:
            # A package without an exit code of its own, or with 0, is one whose executable is missing after the build
            return "not built" if self.BuildExitCode is None or self.BuildExitCode == 0 else f"not built (exit code {self.BuildExitCode})"
        if self.CommandState == CommandState.Failed:
            return f"command failed with exit code {self.CommandExitCode}"
        if self.CommandState == CommandState.CouldNotStart:
            return "command could not be started"
        if self.CommandState == CommandState.TimedOut:
            return "command timed out"
        raise Exception(f"Internal error, package '{self.Name}' did not fail")


class BuildOutcome:
    def __init__(self, packages: Sequence[Package], isDryRun: bool = False) -> None:
        """packages are the packages of the build, in build order. isDryRun: nothing was built or run, so nothing is known about any of them"""
        super().__init__()
        self.IsDryRun = isDryRun
        self.Packages = [PackageOutcome(package) for package in packages]
        # The exit code of the build of all packages, None when the generator builds one package at a time or nothing was built
        self.MasterBuildExitCode: int | None = None
        self.__packageDict = {entry.Package: entry for entry in self.Packages}
        self.__nameDict = {entry.Name: entry for entry in self.Packages}

    def Get(self, package: Package) -> PackageOutcome:
        return self.__packageDict[package]

    def TryGet(self, packageName: str) -> PackageOutcome | None:
        """The outcome of a package by its resolved name (Package.Name), None when the package was not part of the build"""
        return self.__nameDict.get(packageName)

    @property
    def HasMasterBuildFailed(self) -> bool:
        return self.MasterBuildExitCode is not None and self.MasterBuildExitCode != 0

    @property
    def HasFailures(self) -> bool:
        return self.HasMasterBuildFailed or any(entry.HasFailed for entry in self.Packages)

    def SetMasterBuildResult(self, exitCode: int) -> None:
        self.MasterBuildExitCode = exitCode
        if exitCode == 0:
            for entry in self.Packages:
                entry.State = PackageBuildState.Built

    def SetPackageBuildResult(self, package: Package, exitCode: int) -> None:
        entry = self.Get(package)
        entry.BuildExitCode = exitCode
        if exitCode != 0:
            entry.State = PackageBuildState.NotBuilt
            return
        self.__SetBuilt(entry)

    def __SetBuilt(self, entry: PackageOutcome) -> None:
        entry.State = PackageBuildState.Built
        # What the package depends on was built with it or before it. Only the packages nothing is known about are marked: one whose own
        # build command failed stays not built.
        for dependency in entry.Package.ResolvedAllDependencies:
            dependencyEntry = self.__packageDict.get(dependency.Package)
            if dependencyEntry is not None and dependencyEntry.State == PackageBuildState.Unknown:
                dependencyEntry.State = PackageBuildState.Built

    def SetExecutableExists(self, package: Package, exists: bool) -> None:
        """Record what the executable of a package says: without it the package is not built, with it a package nothing was known about is"""
        entry = self.Get(package)
        if not exists:
            entry.State = PackageBuildState.NotBuilt
        elif entry.State == PackageBuildState.Unknown:
            self.__SetBuilt(entry)

    def SetNotVerified(self, package: Package) -> None:
        self.Get(package).State = PackageBuildState.NotVerified

    def SetCommandResult(self, package: Package, command: Sequence[str], exitCode: int | None) -> None:
        """exitCode is None for a command that could not be started"""
        entry = self.Get(package)
        entry.Command = list(command)
        entry.CommandExitCode = exitCode
        if exitCode is None:
            entry.CommandState = CommandState.CouldNotStart
        elif exitCode == 0:
            entry.CommandState = CommandState.Succeeded
        else:
            entry.CommandState = CommandState.Failed

    def SetCommandTimedOut(self, package: Package, command: Sequence[str]) -> None:
        entry = self.Get(package)
        entry.Command = list(command)
        entry.CommandExitCode = None
        entry.CommandState = CommandState.TimedOut

    def FormatSummary(self) -> list[str]:
        """What is printed at the end of a run with '--KeepGoing': one line when nothing failed, else a heading and one line per package that
        failed, in build order.
          Build summary: 12 packages built
          Build summary: 10 of 12 packages built, failed:
          - Demo.App1: not built (exit code 2)
          - Demo.App2: command failed with exit code 3
          Build summary: the build failed with exit code 1, 4 of 6 executables built, failed:
          - Demo.App5: not built (exit code 1)
          - Demo.App6: not known
        A dry run built nothing and says so:
          Build summary: dry run, nothing was built
        """
        if self.IsDryRun:
            return [BuildDryRun.KeepGoingSummary]
        if self.HasMasterBuildFailed:
            executables = [entry for entry in self.Packages if entry.Package.Type == PackageType.Executable]
            builtCount = BuildOutcome.__CountBuilt(executables)
            heading = (
                f"Build summary: the build failed with exit code {self.MasterBuildExitCode}, {builtCount} of {_Count(len(executables), 'executable')} built"
            )
        else:
            builtCount = BuildOutcome.__CountBuilt(self.Packages)
            if builtCount == len(self.Packages):
                heading = f"Build summary: {_Count(builtCount, 'package')} built"
            else:
                heading = f"Build summary: {builtCount} of {_Count(len(self.Packages), 'package')} built"

        failed = [entry for entry in self.Packages if entry.HasFailed]
        if len(failed) <= 0:
            return [heading]
        return [f"{heading}, failed:"] + [f"- {entry.Name}: {entry.DescribeFailure()}" for entry in failed]

    @staticmethod
    def __CountBuilt(entries: Sequence[PackageOutcome]) -> int:
        return len([entry for entry in entries if entry.State == PackageBuildState.Built])


class ExecutableStamps:
    """The executables of the packages as they are before a build, to tell afterwards if the build wrote an executable or an earlier build
    left it. A file is compared with itself (modification time and size), never with a clock, so the resolution of the file times of the
    file system does not matter: at worst a file that was written again looks unchanged, and is then verified.
    """

    def __init__(self) -> None:
        super().__init__()
        self.__stampDict: dict[Package, tuple[int, int] | None] = {}

    @staticmethod
    def __TryGetStamp(path: str | None) -> tuple[int, int] | None:
        """The modification time and size of a file, None when there is no such file"""
        if path is None:
            return None
        try:
            status = os.stat(path)
        except OSError:
            return None
        return (status.st_mtime_ns, status.st_size)

    def Record(self, package: Package, executablePath: str | None) -> None:
        """Record the executable of a package before the build, executablePath is None when the path is not known yet"""
        self.__stampDict[package] = ExecutableStamps.__TryGetStamp(executablePath)

    def IsNewOrChanged(self, package: Package, executablePath: str) -> bool:
        """True when the executable is there now and is not the file that was recorded: the build wrote it"""
        stamp = ExecutableStamps.__TryGetStamp(executablePath)
        # A file the build did not touch always compares equal, so True means the build wrote the file. False does not always mean it did
        # not: a file written again with the same size and within the resolution of the file times of the file it replaced compares equal
        # too. The only effect is that the package is then verified by a build of its own, which finds it up to date: one build tool
        # start more, never a wrong result.
        return stamp is not None and stamp != self.__stampDict.get(package)
