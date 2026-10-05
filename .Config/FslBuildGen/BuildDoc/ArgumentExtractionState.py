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

# 'FslBuildDoc --ExtractArguments ... --Resume': the state an argument extraction leaves on disk and the identity of the run it belongs to.
#
# The state is the argument file each app wrote (ArgumentExtraction, '<directory>/<package name>.json'), which is on disk as soon as the app
# has run, and one file next to them that says which run they belong to: the identity. A run that was stopped, or that ended with apps that
# failed, is run again with '--Resume': an app with a valid argument file is not run again.
#
# The identity holds what has to be equal for the files of an earlier run to be used: what decides which apps are selected and what they
# are built from. A run with another identity never uses them. The gen files and the sources are not part of it: a resume is for the same
# run a little later.
#
# A run that is not a resume removes every argument file of the directory before it writes its identity, so the directory only ever holds
# files of the run its identity names.

import contextlib
import json
import os
from collections.abc import Iterable, Sequence

from FslBuildGen import IOUtil, PathCompare
from FslBuildGen.BuildDoc import ArgumentExtraction
from FslBuildGen.BuildDoc.ArgumentExtraction import JsonDictType
from FslBuildGen.DataTypes import FilterMethod
from FslBuildGen.Generator.GeneratorPlugin import GeneratorPlugin
from FslBuildGen.Log import Log
from FslBuildGen.Tool.ToolAppConfig import ToolAppConfig
from FslBuildGen.ToolConfig import ToolConfig

# The file that holds the identity of the run the argument files belong to. Its extension keeps it apart from the file of any app.
IdentityFileName = "_RunIdentity.fsl"
_g_formatVersion = 1
_g_keyFormatVersion = "FormatVersion"
_g_keyIdentity = "Identity"
_g_argumentFileExtension = ".json"

# The value of the identity that names the directory the apps are selected in
_g_nameApps = "Apps"
# The values that hold a path: two runs in the same directory have the same value, however the directory was typed
_g_pathValueNames = frozenset([_g_nameApps])


def _IsSameValue(name: str, earlier: str | None, current: str | None) -> bool:
    if earlier is None or current is None or name not in _g_pathValueNames:
        return earlier == current
    return PathCompare.IsSameName(earlier, current)


class RunIdentity:
    def __init__(self, values: dict[str, str]) -> None:
        """values: what the run is, a text per name in the order they are shown"""
        super().__init__()
        self.Values = dict(values)

    def GetDifferences(self, earlier: RunIdentity) -> list[tuple[str, str | None, str | None]]:
        """What differs from the identity of an earlier run: the name, the earlier value and the value of this run. A value is None when
        the run does not have it.
        """
        names = list(self.Values) + [name for name in earlier.Values if name not in self.Values]
        return [
            (name, earlier.Values.get(name), self.Values.get(name)) for name in names if not _IsSameValue(name, earlier.Values.get(name), self.Values.get(name))
        ]


class ResumeRefusedException(Exception):
    """'--Resume' of a run that has another identity than the run that left the state"""

    def __init__(self, differences: Sequence[tuple[str, str | None, str | None]]) -> None:
        lines = ["--Resume was refused: the earlier argument extraction ran with other settings, so what it extracted can not be used."]
        lines += [f"- {name}: was {_Quote(earlier)}, is {_Quote(current)}" for name, earlier, current in differences]
        lines.append("Run again with the settings of the earlier run, or without --Resume to start fresh.")
        super().__init__("\n".join(lines))
        self.Differences = list(differences)


def _Quote(value: str | None) -> str:
    return "not set" if value is None else f"'{value}'"


def _GetSelectionText(extractArguments: str, currentDir: str, recursive: bool) -> str:
    """The apps '--ExtractArguments' selects: every app, or the ones of a directory"""
    if extractArguments == "*":
        return "*"
    return f"{extractArguments} {'with -r ' if recursive else ''}in {IOUtil.NormalizePath(currentDir)}"


def CreateRunIdentity(
    toolConfig: ToolConfig,
    generator: GeneratorPlugin,
    toolAppConfig: ToolAppConfig,
    additionalInputDirs: Sequence[str],
    extractArguments: str,
    currentDir: str,
    recursive: bool,
) -> RunIdentity:
    """The identity of an argument extraction.
    - generator is the generator of the platform, toolAppConfig what the command line gave the tool.
    - extractArguments, currentDir and recursive select the apps: '--ExtractArguments', the current directory and '-r'.
    Not part of it, because it changes neither which apps are selected nor what they are built from: the build threads, '--PerAppBuild',
    the verbosity and what FslBuildDoc does besides the extraction. The build directory is not named: the state is stored in it.
    """
    cmakeConfig = generator.CMakeConfig
    if cmakeConfig is None:
        raise Exception("Internal error, the generator is not configured")
    filters = toolAppConfig.BuildPackageFilters
    extensions = "*"
    if filters.ExtensionNameList.FilterMethod != FilterMethod.AllowAll:
        extensions = f"{filters.ExtensionNameList.FilterMethod.name}: {','.join(sorted(str(entry) for entry in filters.ExtensionNameList.Content))}"
    recipes = "*"
    if not filters.RecipeFilterManager.AllRecipesEnabled:
        recipeNames = sorted(f"{'+' if entry.Enabled else '-'}{entry.Name}" for entry in filters.RecipeFilterManager.Content)
        recipes = ",".join((["*"] if filters.RecipeFilterManager.DefaultEnabled else []) + recipeNames)
    variants = sorted(f"{name.Value}={option.Value}" for name, option in toolAppConfig.BuildVariantConstraints.Dict.items())
    variables = sorted(f"{name}={value}" for name, value in toolAppConfig.UserSetVariables.Dict.items())
    return RunIdentity(
        {
            "Tool version": str(toolConfig.ToolVersion),
            "Platform": generator.PlatformName,
            "Generator": toolAppConfig.Generator.name,
            "CMake generator": cmakeConfig.GeneratorName,
            "CMake arguments": " ".join(cmakeConfig.CMakeConfigAppArguments),
            "CMake install prefix": "" if cmakeConfig.InstallPrefix is None else cmakeConfig.InstallPrefix,
            "CMake find_package": "allowed" if cmakeConfig.AllowFindPackage else "not allowed",
            "Variants": ",".join(variants),
            "Variables": ",".join(variables),
            "Features": ",".join(sorted(filters.FeatureNameList)),
            "Required features": ",".join(sorted(filters.RequiredFeatureNameList)),
            "Extensions": extensions,
            "Recipes": recipes,
            "Input directories": ",".join(additionalInputDirs),
            _g_nameApps: _GetSelectionText(extractArguments, currentDir, recursive),
        }
    )


class ArgumentExtractionState:
    def __init__(self, log: Log, argumentFileDirectory: str) -> None:
        """argumentFileDirectory is the directory of the argument files (ArgumentExtraction.GetArgumentFileDirectory)"""
        super().__init__()
        self.Log = log
        self.Directory = argumentFileDirectory
        self.IdentityFilePath = IOUtil.Join(argumentFileDirectory, IdentityFileName)

    def TryReadIdentity(self) -> RunIdentity | None:
        """The identity of the run that left the state. None when there is none or it can not be used: no file, a file that was not
        written to its end, or one of another version of the format.
        """
        try:
            content = IOUtil.TryReadFileUTF8(self.IdentityFilePath)
            if content is None:
                return None
            data = json.loads(content)
        except ValueError:
            return None
        if not isinstance(data, dict) or data.get(_g_keyFormatVersion) != _g_formatVersion:
            return None
        values = data.get(_g_keyIdentity)
        if not isinstance(values, dict) or not all(isinstance(name, str) and isinstance(value, str) for name, value in values.items()):
            return None
        return RunIdentity(values)

    def Begin(self, identity: RunIdentity | None) -> None:
        """Start a run that uses nothing of an earlier one: every argument file of the directory is removed, then the identity of the run
        is written. That order means a run that is stopped in between leaves no file under an identity it was not written for.
        Without an identity no run can be resumed from what this one leaves.
        """
        IOUtil.SafeMakeDirs(self.Directory)
        IOUtil.RemoveFile(self.IdentityFilePath)
        for filename in IOUtil.GetFilesAt(self.Directory, False):
            if filename.endswith(_g_argumentFileExtension):
                IOUtil.RemoveFile(IOUtil.Join(self.Directory, filename))
        if identity is not None:
            content = {_g_keyFormatVersion: _g_formatVersion, _g_keyIdentity: identity.Values}
            IOUtil.WriteFileUTF8(self.IdentityFilePath, json.dumps(content, indent=2) + "\n")

    def TryReadArguments(self, packageName: str) -> JsonDictType | None:
        """The arguments an app wrote in the earlier run, None when it has no valid file: the same check as for a file of this run"""
        return ArgumentExtraction.TryReadArgumentFile(self.Log, ArgumentExtraction.GetArgumentFilePath(self.Directory, packageName))[0]

    def RemoveArguments(self, packageNames: Iterable[str]) -> None:
        for packageName in packageNames:
            IOUtil.RemoveFile(ArgumentExtraction.GetArgumentFilePath(self.Directory, packageName))

    def Remove(self, packageNames: Iterable[str]) -> None:
        """The end of a run that needs no resume: the argument files of its apps and the identity are removed, and the directory when
        nothing is left in it.
        """
        self.RemoveArguments(packageNames)
        IOUtil.RemoveFile(self.IdentityFilePath)
        # Only an empty directory can be removed this way
        with contextlib.suppress(OSError):
            os.rmdir(self.Directory)
