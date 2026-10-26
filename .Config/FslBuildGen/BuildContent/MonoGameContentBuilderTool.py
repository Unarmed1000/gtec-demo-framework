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

# The tool that builds the MonoGame content project of an app (MonoGameContentProject), in the dotnet tool manifest of the app.
#
# MonoGame.Content.Builder.Task, the NuGet package that builds a '.mgcb' in the C# build, runs MGCB as the local dotnet tool 'dotnet-mgcb'. It
# restores the tools of the manifest '.config/dotnet-tools.json' of the project first and goes on when that fails, so without the tool in the
# manifest the build only says that 'dotnet mgcb' exited with code 1. The app does not keep the manifest by hand: generation writes it next to the
# project of the app with the content project, so every tool that generates does.
#
# - The tool has the version of the package the app references. MonoGame releases the two with one version, so they never drift apart. That is
#   one version (SemanticVersionPatternRender): a wildcard or a range is an error.
# - An app that gets a content project but does not reference the package is an error: nothing would build its content.
# - An app with content that references the package on a template without the content project is an error too: the package finds nothing to
#   build, the build only warns, and the app runs on the content an earlier build left.
# - The other tools of a manifest that is there are kept, and so is what else the entry of the tool holds.
# - The file is written only when its text changes.

import json
from collections.abc import Sequence
from typing import Any

from FslBuildGen import IOUtil, SemanticVersionPatternRender
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import PackageExternalDependency
from FslBuildGen.SemanticVersionPatternRender import VersionRenderException

# The NuGet package that builds the content project, and the tool it runs
PackageName = "MonoGame.Content.Builder.Task"
ToolName = "dotnet-mgcb"
_g_toolCommand = "mgcb"
# The manifest of the project, where 'dotnet tool restore' finds it from the directory of the project
ManifestPath = ".config/dotnet-tools.json"


def _TryGetReference(packageReferences: Sequence[PackageExternalDependency]) -> PackageExternalDependency | None:
    """The reference to the package (NuGet names are not case sensitive)"""
    return next((entry for entry in packageReferences if entry.Name.lower() == PackageName.lower()), None)


def CheckNotReferenced(appName: str, templateDescription: str, packageReferences: Sequence[PackageExternalDependency]) -> None:
    """Raise if an app with content, on a template without the MonoGame content project, references the package that builds it"""
    reference = _TryGetReference(packageReferences)
    if reference is not None:
        raise UsageErrorException(
            f"The app '{appName}' has content and references the NuGet package '{reference.Name}' (from '{reference.IntroducedByPackageName}') that "
            f"builds MonoGame content, but {templateDescription} has no MonoGame content project, so its content would never be built and the app "
            'would run on the content an earlier build left. Use a template whose Customization.xml has <AppContent Project="MonoGame"/>, or remove '
            "the reference"
        )


def GetToolVersion(appName: str, packageReferences: Sequence[PackageExternalDependency]) -> str:
    """The version of the tool: the version of the package the app references"""
    reference = _TryGetReference(packageReferences)
    if reference is None:
        raise UsageErrorException(
            f"The app '{appName}' gets a MonoGame content project from its template, but it does not reference the NuGet package '{PackageName}' "
            f"that builds it, so its content would never be built. Give it the ExternalDependency '{PackageName}' Type=\"PackageReference\", "
            "its own or from a package it depends on"
        )
    if reference.Version is None:
        raise UsageErrorException(f"External dependency '{reference.Name}' of package '{appName}' has no version, the tool '{ToolName}' needs one")
    try:
        return SemanticVersionPatternRender.ToDotnetToolVersion(reference.Version)
    except VersionRenderException as ex:
        raise UsageErrorException(
            f"External dependency '{reference.Name}' of package '{appName}' (from '{reference.IntroducedByPackageName}'): {ex}. The MonoGame content "
            f"project is built by the tool '{ToolName}' of the same version, give the package one version, like '3.8.6'"
        ) from ex


def BuildText(filePath: str, lastText: str | None, version: str) -> str:
    """The manifest with the tool of the version, made from the manifest that is there (lastText) or a new one"""
    manifest: Any = {"version": 1, "isRoot": True, "tools": {}}
    if lastText is not None:
        try:
            manifest = json.loads(lastText)
        except json.JSONDecodeError as ex:
            raise UsageErrorException(f"The dotnet tool manifest '{filePath}' is not valid json: {ex}") from ex
        if not isinstance(manifest, dict) or not isinstance(manifest.get("tools", {}), dict):
            raise UsageErrorException(f"The dotnet tool manifest '{filePath}' is not a manifest: it is not an object with an object 'tools'")
    tools = manifest.setdefault("tools", {})
    entry = tools.get(ToolName)
    entry = dict(entry) if isinstance(entry, dict) else {}
    entry["version"] = version
    entry["commands"] = [_g_toolCommand]
    tools[ToolName] = entry
    return json.dumps(manifest, indent=2) + "\n"


def Write(log: Log, configDisableWrite: bool, packagePath: str, version: str) -> str:
    """Write the tool into the manifest of the project in packagePath, when its text changed. Returns the path of the manifest"""
    filePath = IOUtil.Join(packagePath, ManifestPath)
    lastText = IOUtil.ReadFileUTF8(filePath) if IOUtil.IsFile(filePath) else None
    text = BuildText(filePath, lastText, version)
    if configDisableWrite:
        log.LogPrint(f"Would write the dotnet tool manifest '{filePath}'")
        return filePath
    IOUtil.SafeMakeDirs(IOUtil.GetDirectoryName(filePath))
    if IOUtil.WriteFileUTF8IfChanged(filePath, text):
        log.LogPrint(f"Wrote '{ToolName}' {version} into the dotnet tool manifest '{filePath}'")
    return filePath
