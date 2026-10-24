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

# The content of an app: its own, and the content of every library it uses.
#
# Content is part of a package like its source is. A library's 'Content' and 'Content.bld' reach the apps that use the library by being
# there, the way a header is public by being in 'include'. There is no private content: a library has no content directory of its own
# when the app runs, so content that reaches no app is read by nothing.
#
# - Everything is merged at the app. A file keeps the path it has below the content directory of its owner, so the owner decides the path
#   and the app maps nothing.
# - It reaches every app that has the owner in its build order. A private dependency on the way does not stop it: a header is needed where
#   it is compiled, content is needed when the code of its owner runs, wherever that code is linked into.
# - A file of 'Content.bld' is built by the app: the features of the app pick the content builder, so the same source can be built
#   differently for two apps. Only the output is content of the app.
# - Two files on one path are an error, also when one of them is the app's own. So are two paths that only differ in case, in the name or
#   in a directory on the way: they can not both exist on a file system that ignores case. The case a path has is kept as it is.
# - A library with 'UseContentNamespace' puts everything it has below its package name: '<PackageName>/<path>', for the files of 'Content',
#   the files a command copies in and every output of 'Content.bld'. The library opts in, the app has no say, so libraries that ship the
#   same paths can not collide. The name is the one the package declares, which every flavor instance of it shares.
# - Each file says what it is: its content type and the profile its owner gives the file it comes from (ContentProfileList), so the
#   importer of a platform and a content builder build it the way its owner means.

from FslBuildGen.BuildContent import ContentBuilder, ContentBuildSteps
from FslBuildGen.BuildContent.PathRecord import PathRecord
from FslBuildGen.DataTypes import PackageType
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package
from FslBuildGen.Packages.PackageAppContent import PackageAppContentEntry, PackageAppContentKind
from FslBuildGen.ToolConfig import ToolConfig


def GetContentOwners(package: Package) -> list[Package]:
    """The packages whose content is content of the app: the app, and the libraries of its build order that have content directories"""
    owners = [package]
    for entry in package.ResolvedBuildOrder:
        if entry is not package and entry.Type == PackageType.Library and not entry.IsVirtual:
            owners.append(entry)
    return owners


def Resolve(log: Log, toolConfig: ToolConfig, package: Package) -> list[PackageAppContentEntry]:
    """The content of the app, sorted by its path in the app. The content lists of the app and of the packages of its build order
    must be resolved.
    """
    featureList = [entry.Name for entry in package.ResolvedAllUsedFeatures]
    contentProcessorManager: ContentBuilder.ContentProcessorManager | None = None

    result: list[PackageAppContentEntry] = []
    # A package with flavors can be in the build order as more than one instance, they share their directory: each file once
    added: set[tuple[str, str]] = set()

    def Add(
        owner: Package,
        source: PathRecord,
        appPath: str,
        kind: PackageAppContentKind,
        builderName: str | None = None,
        builderArguments: list[str] | None = None,
    ) -> None:
        if owner.UseContentNamespace:
            appPath = f"{owner.NameInfo.SourceName}/{appPath}"
        key = (source.ResolvedPath, appPath)
        if key not in added:
            added.add(key)
            profile = owner.ResolvedContentProfiles.get(source.ResolvedPath)
            result.append(
                PackageAppContentEntry(owner.Name, source.SourceRoot.ResolvedPath, source.ResolvedPath, appPath, kind, builderName, builderArguments, profile)
            )

    for owner in GetContentOwners(package):
        for contentFile in owner.ResolvedContentFiles:
            Add(owner, contentFile, contentFile.RelativePath, PackageAppContentKind.Content)
        for syncFile in owner.ResolvedContentBuilderSyncInputFiles or []:
            Add(owner, syncFile, syncFile.RelativePath, PackageAppContentKind.Synced)
        buildFiles = owner.ResolvedContentBuilderBuildInputFiles or []
        if len(buildFiles) > 0:
            if contentProcessorManager is None:
                contentProcessorManager = ContentBuilder.GetContentProcessorManager(log, toolConfig, featureList, package.ResolvedPlatformName)
            for buildFile in buildFiles:
                # The app builds it, with the builders its features and its platform select and the build rules of the owner. A file
                # nothing builds for this app is not content of it, as in the app's own directory
                for step in ContentBuildSteps.FindBuildSteps(log, contentProcessorManager, buildFile, owner.ResolvedContentBuildRules):
                    Add(owner, buildFile, step.OutputRelativePath, PackageAppContentKind.Built, step.BuilderName, step.ToolArguments)

    result.sort(key=lambda entry: (entry.AppPath, entry.SourcePath))
    Check(package.Name, result)
    return result


def _Describe(entry: PackageAppContentEntry) -> str:
    return f"'{entry.AppPath}' from '{entry.SourcePath}' (package '{entry.OwnerName}')"


def Check(appName: str, entries: list[PackageAppContentEntry]) -> None:
    """Stops with every path of the content of the app that two files claim, and every pair of paths that only differ in case"""
    problems: list[str] = []
    hasCollision = False
    hasCaseProblem = False
    byPath: dict[str, PackageAppContentEntry] = {}
    # Each path and each directory on the way to it, without its case
    spellingById: dict[str, tuple[str, PackageAppContentEntry]] = {}
    reported: set[tuple[str, str]] = set()
    for entry in entries:
        existing = byPath.get(entry.AppPath)
        if existing is not None:
            problems.append(f"- two files on one path: {_Describe(existing)} and {_Describe(entry)}")
            hasCollision = True
            continue
        byPath[entry.AppPath] = entry

        parts = entry.AppPath.split("/")
        for index in range(len(parts)):
            spelling = "/".join(parts[: index + 1])
            known = spellingById.get(spelling.lower())
            if known is None:
                spellingById[spelling.lower()] = (spelling, entry)
            elif known[0] != spelling:
                # Told once, where the two part: everything below a directory that is spelled two ways differs too
                if (known[0], spelling) not in reported:
                    reported.add((known[0], spelling))
                    problems.append(f"- '{known[0]}' and '{spelling}' only differ in case: {_Describe(known[1])} and {_Describe(entry)}")
                    hasCaseProblem = True
                break

    if len(problems) > 0:
        # What to do about the kinds of problem that were found
        endings: list[str] = []
        if hasCollision:
            endings.append(
                "A path in the content of an app belongs to one file: rename or remove one of each pair in the package that owns it. A library "
                'can also put all of its content below its package name, with UseContentNamespace="true" on its element in its gen file.'
            )
        if hasCaseProblem:
            endings.append(
                "Two paths that only differ in case can not both exist on a file system that ignores case: rename one of each pair in the package that owns it."
            )
        raise UsageErrorException(f"The content of '{appName}' can not be put into one directory:\n" + "\n".join(problems) + "\n" + "\n".join(endings))
