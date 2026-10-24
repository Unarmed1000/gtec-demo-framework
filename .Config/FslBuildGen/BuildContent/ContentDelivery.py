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

# Puts the content of an app into the one directory the app reads it from: its resolved content directory.
#
# The content of an app is its own and that of the libraries it uses (PackageAppContentResolver). It is not put into the 'Content' directory
# of the package, which holds what its author made: it goes to 'Content' in the build directory of the app for the platform
# ('<app>/build/<platform>/Content'), or where a build system asks for it. The app's own files are copied there too, so the directory is
# complete and every relative path in it works, also the ones in a file a content tool of the app reads there.
#
# - A file of a 'Content' directory is copied, a file a command copies in is copied, a file of a 'Content.bld' directory is built.
# - What was delivered is written down (the manifest, next to the directory). A file of the last delivery that is not part of this one is
#   removed: its owner dropped it, or the app no longer uses the owner. Nothing else in the directory is touched: a tool that builds from
#   the directory may keep its own files there.
# - A copy is made when the file is not there, or when the source is not the file that was copied last (another path, length or time).

import json
import os
import shutil
from collections.abc import Sequence

from FslBuildGen import IOUtil, TextFileReader, ToolSharedValues
from FslBuildGen.BuildContent import ContentBuilder, MonoGameContentProject
from FslBuildGen.BuildContent.ContentRootRecord import ContentRootRecord
from FslBuildGen.BuildContent.PathRecord import PathRecord
from FslBuildGen.BuildContent.Processor.ListedSourceContent import ListedBuildStep, ListedSourceContent
from FslBuildGen.DataTypes import PackageType
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package
from FslBuildGen.Packages.PackageAppContent import PackageAppContentEntry, PackageAppContentKind
from FslBuildGen.ToolConfig import ToolConfig

ManifestFileName = "_ContentDelivered.json"
_g_manifestVersion = 1


class _CopiedFile:
    def __init__(self, sourcePath: str, length: int, modifiedTimeNs: int) -> None:
        super().__init__()
        self.SourcePath = sourcePath
        self.Length = length
        self.ModifiedTimeNs = modifiedTimeNs


class _Manifest:
    """What the last delivery put into the directory, by the path below it"""

    def __init__(self) -> None:
        super().__init__()
        # The files that were copied, with the file each of them is a copy of
        self.Copied: dict[str, _CopiedFile] = {}
        # Every file of the delivery: the copies and what the content builder made
        self.Files: set[str] = set()

    @staticmethod
    def TryLoad(log: Log, filename: str) -> _Manifest:
        manifest = _Manifest()
        content = TextFileReader.TryReadUTF8OrLocaleIfExists(log, filename, "content manifest", skipBom=True, warn=False)
        if content is None:
            return manifest
        try:
            jsonContent = json.loads(content)
            if jsonContent["Version"] != _g_manifestVersion:
                raise ValueError("unsupported version")
            for path, entry in jsonContent["Copied"].items():
                manifest.Copied[str(path)] = _CopiedFile(str(entry["Source"]), int(entry["Length"]), int(entry["ModifiedTimeNs"]))
            manifest.Files = {str(path) for path in jsonContent["Files"]}
        except ValueError, KeyError, TypeError, AttributeError:
            # Without it nothing of an earlier delivery is removed and every file is copied again, which is always safe
            log.DoPrintWarning(f"The content manifest '{filename}' could not be read, the content is delivered as if for the first time")
            return _Manifest()
        return manifest

    def Save(self, filename: str) -> None:
        jsonContent = {
            "Version": _g_manifestVersion,
            "Copied": {
                path: {"Source": entry.SourcePath, "Length": entry.Length, "ModifiedTimeNs": entry.ModifiedTimeNs}
                for path, entry in sorted(self.Copied.items())
            },
            "Files": sorted(self.Files),
        }
        IOUtil.WriteFileUTF8IfChanged(filename, json.dumps(jsonContent, ensure_ascii=False, indent=2))


def GetResolvedContentPath(package: Package) -> str:
    """The directory the content of the app is delivered to when no build system names one"""
    return IOUtil.Join(GetPackageBuildPath(package), ToolSharedValues.CONTENT_FOLDER_NAME)


def GetResolvedContentRelativePath(package: Package) -> str:
    """The resolved content directory relative to the directory of the package ('build/Windows/Content'), for a project file"""
    if package.ResolvedBuildPath is None:
        raise Exception(f"Package '{package.Name}' has no build directory")
    return IOUtil.Join(package.ResolvedBuildPath, ToolSharedValues.CONTENT_FOLDER_NAME)


def GetPackageBuildPath(package: Package) -> str:
    if package.AbsolutePath is None or package.ResolvedBuildPath is None:
        raise Exception(f"Package '{package.Name}' has no build directory")
    return IOUtil.Join(package.AbsolutePath, package.ResolvedBuildPath)


def GetInheritedContent(package: Package) -> list[PackageAppContentEntry]:
    """The content of the app that is not its own"""
    return [entry for entry in package.ResolvedAppContent if entry.OwnerName != package.Name]


def CheckNothingIsInherited(package: Package, generatorName: str) -> None:
    """For a generator whose build files run the app where its package is: it can only give the app its own content"""
    inherited = GetInheritedContent(package)
    if len(inherited) > 0:
        owners = sorted({entry.OwnerName for entry in inherited})
        raise UsageErrorException(
            f"The executable '{package.Name}' gets content from {', '.join(owners)} ({len(inherited)} files, the first is '{inherited[0].AppPath}'). "
            f"The generator '{generatorName}' builds and runs an app in the directory of its package and can not give it the content of "
            "another package. Use another generator for it"
        )


def _ToPathRecord(log: Log, entry: PackageAppContentEntry) -> PathRecord:
    return PathRecord(log, ContentRootRecord(log, entry.SourceRootPath), entry.SourcePath)


def _CopyFiles(
    log: Log, configDisableWrite: bool, contentOutputPath: str, entries: Sequence[PackageAppContentEntry], lastManifest: _Manifest, manifest: _Manifest
) -> tuple[int, int]:
    """Copy the files that are used as they are, returns the number of files copied and the number that were up to date"""
    copied = 0
    upToDate = 0
    for entry in entries:
        destinationPath = IOUtil.Join(contentOutputPath, entry.AppPath)
        sourceStat = os.stat(entry.SourcePath)
        record = _CopiedFile(entry.SourcePath, sourceStat.st_size, sourceStat.st_mtime_ns)
        last = lastManifest.Copied.get(entry.AppPath)
        isCurrent = (
            last is not None
            and last.SourcePath == record.SourcePath
            and last.Length == record.Length
            and last.ModifiedTimeNs == record.ModifiedTimeNs
            and os.path.isfile(destinationPath)
            and os.path.getsize(destinationPath) == record.Length
        )
        if isCurrent:
            upToDate += 1
        else:
            log.LogPrintVerbose(2, f"Copying '{entry.SourcePath}' to '{destinationPath}'")
            if not configDisableWrite:
                IOUtil.SafeMakeDirs(IOUtil.GetDirectoryName(destinationPath))
                shutil.copyfile(entry.SourcePath, destinationPath)
            copied += 1
        manifest.Copied[entry.AppPath] = record
        manifest.Files.add(entry.AppPath)
    return (copied, upToDate)


def _RemoveFilesOfTheLastDelivery(log: Log, configDisableWrite: bool, contentOutputPath: str, lastManifest: _Manifest, manifest: _Manifest) -> int:
    """Remove what the last delivery put there and this one did not, and the directories that are empty after that"""
    removed = 0
    for path in sorted(lastManifest.Files - manifest.Files):
        filename = IOUtil.Join(contentOutputPath, path)
        if os.path.isfile(filename):
            log.LogPrintVerbose(2, f"Removing '{filename}', it is no longer content of the app")
            if not configDisableWrite:
                os.remove(filename)
                directory = IOUtil.GetDirectoryName(filename)
                while len(directory) > len(contentOutputPath) and len(os.listdir(directory)) <= 0:
                    os.rmdir(directory)
                    directory = IOUtil.GetDirectoryName(directory)
            removed += 1
    return removed


def Deliver(
    log: Log,
    configDisableWrite: bool,
    toolConfig: ToolConfig,
    packagePath: str,
    packageBuildPath: str,
    contentOutputPath: str,
    featureList: list[str],
    entries: Sequence[PackageAppContentEntry],
    dependencyFile: str | None = None,
    platformName: str | None = None,
) -> None:
    """Deliver the entries into contentOutputPath.
    platformName: the platform the content is built for, it picks the content builders with the features
    packagePath: the directory of the app, for what is told about the delivery
    packageBuildPath: where the state of the content builder is kept
    dependencyFile: a make style dependency file to write for the build system, see ContentBuilder.Builder
    """
    manifestFilename = IOUtil.Join(IOUtil.GetDirectoryName(contentOutputPath), ManifestFileName)
    lastManifest = _Manifest.TryLoad(log, manifestFilename)
    if len(entries) <= 0 and len(lastManifest.Files) <= 0:
        log.LogPrintVerbose(1, f"'{packagePath}' has no content")
        return
    if not configDisableWrite:
        IOUtil.SafeMakeDirs(packageBuildPath)
        IOUtil.SafeMakeDirs(contentOutputPath)

    manifest = _Manifest()
    copied, upToDate = _CopyFiles(
        log, configDisableWrite, contentOutputPath, [entry for entry in entries if entry.Kind == PackageAppContentKind.Content], lastManifest, manifest
    )

    built = 0
    builderEntries = [entry for entry in entries if entry.Kind != PackageAppContentKind.Content]
    syncFiles = [_ToPathRecord(log, entry) for entry in builderEntries if entry.Kind == PackageAppContentKind.Synced]
    # A file of 'Content.bld' is built once for each output a build rule gives it
    buildSteps: dict[str, list[ListedBuildStep]] = {}
    buildFiles: list[PathRecord] = []
    for entry in builderEntries:
        if entry.Kind == PackageAppContentKind.Built:
            if entry.SourcePath not in buildSteps:
                buildSteps[entry.SourcePath] = []
                buildFiles.append(_ToPathRecord(log, entry))
            buildSteps[entry.SourcePath].append(ListedBuildStep(entry.AppPath, entry.BuilderName, entry.BuilderArguments, entry.Profile))
    # What the content builder is to deliver is part of the delivery whether it has to do something for it or not
    manifest.Files.update(entry.AppPath for entry in builderEntries)
    if configDisableWrite:
        # The content builder keeps the state of what it made, so it is not run when nothing may be written: what it would do is told
        built = len(buildFiles)
        copied += len(syncFiles)
    elif len(builderEntries) > 0:
        contentBuildPath = IOUtil.Join(packagePath, toolConfig.ContentDirectory.BuildName)
        builder = ContentBuilder.Builder(
            log,
            configDisableWrite,
            toolConfig,
            packageBuildPath,
            contentBuildPath,
            contentOutputPath,
            ContentBuilder.GetContentProcessorManager(log, toolConfig, featureList, platformName),
            False,
            dependencyFile,
            listedContent=ListedSourceContent(log, contentOutputPath, contentBuildPath, syncFiles, buildFiles, buildSteps),
        )
        built = builder.Result.Built
        copied += builder.Result.Synced
        upToDate += builder.Result.UpToDate

    removed = _RemoveFilesOfTheLastDelivery(log, configDisableWrite, contentOutputPath, lastManifest, manifest)
    if not configDisableWrite:
        manifest.Save(manifestFilename)

    # Always report what was done, a build log should show that the content was delivered
    summary = f"Content '{packagePath}': {built} built, {copied} copied, {upToDate} up to date"
    if removed > 0:
        summary += f", {removed} removed"
    log.DoPrint(f"{summary} -> '{contentOutputPath}'")


def DeliverPackage(
    log: Log,
    configBuildDir: str,
    configDisableWrite: bool,
    toolConfig: ToolConfig,
    package: Package,
    generatorName: str,
    generatorDeliversAppContent: bool,
    outputPath: str | None = None,
    dependencyFile: str | None = None,
    monoGameContentProject: bool = False,
) -> None:
    """Deliver the content of an app the way its generator has it.
    generatorDeliversAppContent: False for a generator whose app reads the 'Content' directory of its package: only what the app builds itself
                                 is put there, as before, and an app that gets content of another package is refused
    outputPath: the directory a build system asks for. It copies the files that are used as they are itself, so only what is built and what
                a command copies in is delivered there
    monoGameContentProject: also write the MonoGame content project of the app next to its content (MonoGameContentProject), for the
                            template of an app that MonoGame builds the content of
    """
    if package.Type != PackageType.Executable or package.AbsolutePath is None or package.Path is None:
        log.DoPrint(f"'{package.Name}' is not an executable: the content of a library is delivered with the apps that use it")
        return
    if monoGameContentProject:
        CheckMonoGameContentProject(package, generatorName, generatorDeliversAppContent, outputPath)
    featureList = [entry.Name for entry in package.ResolvedAllUsedFeatures]
    if not generatorDeliversAppContent:
        CheckNothingIsInherited(package, generatorName)
        ContentBuilder.Build(
            log, configBuildDir, configDisableWrite, toolConfig, package.Path, featureList, outputPath, dependencyFile, package.ResolvedPlatformName
        )
        return

    entries = package.ResolvedAppContent
    if outputPath is not None:
        # The state of the content builder stays where a build system has had it
        entries = [entry for entry in entries if entry.Kind != PackageAppContentKind.Content]
        packageBuildPath = IOUtil.Join(package.AbsolutePath, configBuildDir)
        Deliver(
            log,
            configDisableWrite,
            toolConfig,
            package.AbsolutePath,
            packageBuildPath,
            outputPath,
            featureList,
            entries,
            dependencyFile,
            package.ResolvedPlatformName,
        )
        return
    Deliver(
        log,
        configDisableWrite,
        toolConfig,
        package.AbsolutePath,
        GetPackageBuildPath(package),
        GetResolvedContentPath(package),
        featureList,
        entries,
        dependencyFile,
        package.ResolvedPlatformName,
    )

    if monoGameContentProject:
        WriteMonoGameContentProject(log, configDisableWrite, package)


def CheckMonoGameContentProject(package: Package, generatorName: str, generatorDeliversAppContent: bool, outputPath: str | None) -> None:
    """Stops when the content project can not be next to the content of the app"""
    if not generatorDeliversAppContent:
        raise UsageErrorException(
            f"The MonoGame content project of '{package.Name}' lists the content in its resolved content directory, and the generator "
            f"'{generatorName}' does not deliver it there"
        )
    if outputPath is not None:
        raise UsageErrorException(
            f"The MonoGame content project of '{package.Name}' is written next to its resolved content directory, not into a directory a "
            "build system asks for: leave out the output directory"
        )


def WriteMonoGameContentProject(log: Log, configDisableWrite: bool, package: Package) -> None:
    """Write the MonoGame content project of the app into its resolved content directory. Also when the content was delivered by the tool
    that runs the build (FslBuild), which is why it is apart from the delivery
    """
    MonoGameContentProject.Write(log, configDisableWrite, GetResolvedContentPath(package), package.Name, package.ResolvedAppContent)
