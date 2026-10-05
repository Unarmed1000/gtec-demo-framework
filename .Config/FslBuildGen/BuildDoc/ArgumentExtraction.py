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

# 'FslBuildDoc --ExtractArguments': what happened to each app whose command line arguments were to be extracted, and the summary of it.
#
# An app is built, then run with '--System.Arguments.Save <file> -h': it writes its arguments to the file as JSON and exits. The file of an
# app is '<directory>/<package name>.json' in the build directory (GetArgumentFileDirectory), never in the source tree. A package name is
# unique and only holds letters, digits, '_' and '.', so two apps can not write the same file.
#
# Every app that was selected ends in exactly one state (ExtractionState). Only an app in the state Extracted gets the argument section of
# its README.md written again.

import json
from collections.abc import Sequence
from enum import Enum
from typing import Any

from FslBuildGen import IOUtil, PathCompare, TextFileReader
from FslBuildGen.Build.BuildOutcome import BuildOutcome, CommandState
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package

# The exit code of FslBuildDoc when the arguments of an app that was not excluded could not be extracted
FailureExitCode = 1
# An app that does not return is stopped after this many seconds. An app that is asked for its arguments writes them and exits before it
# opens a window, which takes well under a second: the limit only has to be far above that on a busy machine.
RunTimeoutSeconds = 60
# The directory of the argument files below the cache directory of the build
ArgumentDirectoryName = "FslBuildDoc"
# Stands for the package name in the argument that names the file, the builder replaces it for each app
PackageNamePlaceholder = "(PACKAGE_NAME)"

JsonDictType = dict[str, Any]


def FormatAppCount(count: int) -> str:
    """A number of apps for a text: '1 app', '2 apps'"""
    return f"{count} app" if count == 1 else f"{count} apps"


class ConfigureFailedException(Exception):
    """The configure of the one build of all apps failed: nothing was built, so no arguments were extracted"""

    def __init__(self, appCount: int, exitCode: int) -> None:
        super().__init__(
            f"The configure of the build of the {FormatAppCount(appCount)} failed with exit code {exitCode}, so no arguments were extracted. "
            "An app that can not be configured stops the build of all of them. Either leave that app out (with --UseFeatures, or with "
            '<BuildDocConfiguration><Requirement Name="..." Skip="true"/></BuildDocConfiguration> in Project.gen), or run again with '
            "--PerAppBuild to build the apps one at a time."
        )
        self.AppCount = appCount
        self.ExitCode = exitCode


class ExtractionState(Enum):
    # The app was built and run and wrote its arguments
    Extracted = 0
    # Left out before the build: the app can not be built for the platform or the project says to skip it
    Excluded = 1
    NotBuilt = 2
    # The app was built but running it failed: it returned an exit code other than 0 or could not be started
    RunFailed = 3
    # The app ran and returned 0 but there is no usable file: it is missing, it is not JSON or it holds no arguments
    NoValidFile = 4
    # The app did not return within the time limit and was stopped
    TimedOut = 5


# The order and the text of the states in the first line of the summary
_g_stateTexts = [
    (ExtractionState.Extracted, "extracted"),
    (ExtractionState.NotBuilt, "not built"),
    (ExtractionState.RunFailed, "failed to run"),
    (ExtractionState.NoValidFile, "without a valid argument file"),
    (ExtractionState.TimedOut, "timed out"),
    (ExtractionState.Excluded, "excluded"),
]


class AppExtraction:
    def __init__(self, package: Package, state: ExtractionState, reason: str, arguments: JsonDictType | None = None, fromEarlierRun: bool = False) -> None:
        """reason says why the app is in a state other than Extracted, for the summary.
        fromEarlierRun: the arguments are the ones an earlier run extracted ('--Resume'), the app was not run again.
        """
        super().__init__()
        self.Package = package
        self.Name = package.Name
        self.State = state
        self.Reason = reason
        self.Arguments = arguments
        self.FromEarlierRun = fromEarlierRun

    @property
    def HasFailed(self) -> bool:
        """True when the arguments should have been extracted and were not"""
        return self.State != ExtractionState.Extracted and self.State != ExtractionState.Excluded


class ArgumentExtractionReport:
    def __init__(self, apps: Sequence[AppExtraction]) -> None:
        """apps are the apps that were selected, in the order they are to be reported in"""
        super().__init__()
        self.Apps = list(apps)

    @property
    def HasFailures(self) -> bool:
        return any(entry.HasFailed for entry in self.Apps)

    def GetArguments(self) -> dict[Package, JsonDictType]:
        """The arguments of the apps they were extracted for"""
        return {entry.Package: entry.Arguments for entry in self.Apps if entry.State == ExtractionState.Extracted and entry.Arguments is not None}

    def FormatSummary(self, nameSharedReasons: bool = False) -> list[str]:
        """What is printed after an extraction: the number of apps per state, then one line per app that failed, in the order of the apps,
        then the excluded apps. Apps that are excluded for the same reason share a line, they are named with nameSharedReasons.
          Argument extraction: 233 extracted, 2 not built, 1 failed to run, 4 excluded
          - Demo.App5: not built (exit code 1)
          - Demo.App2: failed to run (exit code 3)
          - 3 apps excluded: not supported on this platform
            'Demo.A', 'Demo.B', 'Demo.C'
          - Demo.OpenVX101: excluded: the requirement 'OpenVX' is skipped
        After a '--Resume' the first line says how many of the extracted apps were not run again:
          Argument extraction: 233 extracted (230 from the earlier run), 2 not built
        """
        counts = [f"{len([entry for entry in self.Apps if entry.State == state])} {text}" for state, text in _g_stateTexts]
        fromEarlierRun = len([entry for entry in self.Apps if entry.State == ExtractionState.Extracted and entry.FromEarlierRun])
        if fromEarlierRun > 0:
            counts[0] += f" ({fromEarlierRun} from the earlier run)"
        # The apps that were extracted are always counted, the other states only when an app is in them
        lines = ["Argument extraction: " + ", ".join(entry for index, entry in enumerate(counts) if index == 0 or not entry.startswith("0 "))]
        lines += [f"- {entry.Name}: {entry.Reason}" for entry in self.Apps if entry.HasFailed]

        excludedByReason: dict[str, list[str]] = {}
        for entry in self.Apps:
            if entry.State == ExtractionState.Excluded:
                excludedByReason.setdefault(entry.Reason, []).append(entry.Name)
        for reason, names in excludedByReason.items():
            if len(names) == 1:
                lines.append(f"- {names[0]}: excluded: {reason}")
            else:
                lines.append(f"- {FormatAppCount(len(names))} excluded: {reason}")
                if nameSharedReasons:
                    lines.append("  " + ", ".join(f"'{name}'" for name in names))
        return lines


def GetArgumentFileDirectory(buildCacheDirectory: str) -> str:
    """The directory of the argument files, buildCacheDirectory is the cache directory of the build"""
    return IOUtil.Join(buildCacheDirectory, ArgumentDirectoryName)


def GetArgumentFilePath(argumentFileDirectory: str, packageName: str) -> str:
    return IOUtil.Join(argumentFileDirectory, f"{packageName}.json")


def GetRunArguments(argumentFileDirectory: str) -> list[str]:
    """The command that is run for each app, as arguments so a directory with a space in its name is one argument. The builder replaces
    '(EXE)' with the executable and the package name placeholder with the name of the package.
    """
    return ["(EXE)", "--System.Arguments.Save", GetArgumentFilePath(argumentFileDirectory, PackageNamePlaceholder), "-h"]


def SelectApps(apps: Sequence[Package], extractArguments: str, currentDir: str, recursive: bool) -> list[Package]:
    """The apps '--ExtractArguments <extractArguments>' asks for: '*' is every app, anything else ('.') is the app in the current directory
    and with recursive (-r) every app below it as well.
    """
    if extractArguments == "*":
        return list(apps)
    # The current directory is what the user (or the shell) typed, the path of a package is what the tool made of the package location:
    # they are compared the way the file system does ('e:\work' is 'E:/Work' on Windows)
    return [
        app
        for app in apps
        if app.AbsolutePath is not None
        and (PathCompare.IsSamePath(app.AbsolutePath, currentDir) or (recursive and PathCompare.IsBelow(app.AbsolutePath, currentDir)))
    ]


def ReadJsonFile(log: Log, filename: str) -> Any:
    """Read the file an app wrote: the JSON value it holds. Raises ValueError for text that is not JSON and UsageErrorException for a file
    that can not be decoded.
    """
    # The demo app writes the file. It is read as UTF-8, a byte order mark is accepted and a file in the locale encoding is read with a warning
    return json.loads(TextFileReader.ReadUTF8OrLocale(log, filename, "command line argument file", skipBom=True))


def TryReadArgumentFile(log: Log, path: str) -> tuple[JsonDictType | None, str]:
    """Read the file an app wrote. Returns the content, or None and why the file can not be used"""
    if not IOUtil.IsFile(path):
        return (None, "the app wrote no argument file")
    try:
        content = ReadJsonFile(log, path)
    except ValueError as ex:
        return (None, f"the argument file is not valid JSON: {ex}")
    except UsageErrorException as ex:
        return (None, f"the argument file can not be read: {ex}")
    if not isinstance(content, dict) or not isinstance(content.get("arguments"), list):
        return (None, "the argument file holds no 'arguments' list")
    return (content, "")


def CreateAppExtraction(log: Log, package: Package, outcome: BuildOutcome, argumentFileDirectory: str, timeoutSeconds: float) -> AppExtraction:
    """The state of an app after the build that was to build and run it"""
    packageOutcome = outcome.TryGet(package.Name)
    if packageOutcome is None:
        return AppExtraction(package, ExtractionState.NotBuilt, "not built (the package is not part of the build)")
    if packageOutcome.HasBuildFailed:
        return AppExtraction(package, ExtractionState.NotBuilt, packageOutcome.DescribeFailure())
    if packageOutcome.CommandState == CommandState.Failed:
        return AppExtraction(package, ExtractionState.RunFailed, f"failed to run (exit code {packageOutcome.CommandExitCode})")
    if packageOutcome.CommandState == CommandState.CouldNotStart:
        return AppExtraction(package, ExtractionState.RunFailed, "failed to run (the app could not be started)")
    if packageOutcome.CommandState == CommandState.TimedOut:
        return AppExtraction(package, ExtractionState.TimedOut, f"timed out (stopped after {timeoutSeconds:g} seconds)")
    if packageOutcome.CommandState == CommandState.NotRun:
        return AppExtraction(package, ExtractionState.RunFailed, "failed to run (the app was not run)")
    arguments, reason = TryReadArgumentFile(log, GetArgumentFilePath(argumentFileDirectory, package.Name))
    if arguments is None:
        return AppExtraction(package, ExtractionState.NoValidFile, f"no valid argument file ({reason})")
    return AppExtraction(package, ExtractionState.Extracted, "", arguments)
