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

# The gen file scan of FslBuildCheck: is the schema reference of a gen file the one its project expects?
#
# The gen files are found by walking directories. No package is loaded, so the scan works for a gen file that can not be loaded, it does not
# depend on the platform or the features, and it covers the packages no platform uses.
#
# A gen file with a wrong reference is listed. A repair writes the expected reference into it and changes no other byte: the file is read
# and written as bytes, and only the value of the attribute is replaced (GenFileSchemaReference).
# - The reference for an older schema version is valid, such a file is left alone.
# - A file without a schema reference, a file the reference can not be located in for sure, a file that can not be read and a file outside
#   every project are reported and never repaired.

import os

from FslBuildGen import GenFileSchema, GenFileSchemaReference, IOUtil, Util
from FslBuildGen import Main as MainFlow
from FslBuildGen.BuildConfig.BuildUtil import BuildUtil
from FslBuildGen.Config import Config
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.GenFileSchema import ReferenceClass
from FslBuildGen.GenFileSchemaReference import GenFileSchemaReferenceError, ReferenceStatus
from FslBuildGen.Log import Log
from FslBuildGen.ToolConfig import ToolConfig, ToolConfigPackageLocation
from FslBuildGen.ToolConfigPackageProjectContextUtil import ToolConfigPackageProjectContextUtil
from FslBuildGen.ToolConfigProjectContext import ToolConfigProjectContext
from FslBuildGen.ToolMinimalConfig import ToolMinimalConfig


class ScanGenFilesCounts:
    """What a scan found. Every gen file is either checked or not checked."""

    def __init__(self) -> None:
        super().__init__()
        # The gen files whose schema reference was compared to the expected one
        self.Checked = 0
        # The checked files whose reference is wrong
        self.Wrong = 0
        # The wrong files that were written with the expected reference
        self.Repaired = 0
        # The gen files that could not be checked: no reference, not supported, not readable or outside every project
        self.NotChecked = 0


def GetMinimalConfig(toolConfig: ToolConfig) -> ToolMinimalConfig:
    """The root directories and the directories a search for gen files skips: the template directories and the default build directory of the
    project. The other checks also skip the build directories of the active generator, the scan does not depend on a platform.
    """
    toolMiniConfig = toolConfig.GetMinimalConfig(None)
    toolMiniConfig.IgnoreDirectories.append(BuildUtil.GetBuildDir(toolConfig.ProjectInfo, toolConfig.CMakeConfiguration.DefaultBuildDir))
    return toolMiniConfig


def _AddGenFilesBelow(rGenFiles: list[str], directory: str, genFileName: str, skipDirectories: set[str]) -> None:
    """Add the gen file of every package directory below the directory, with the directory rules of the package loader"""
    try:
        subDirectoryNames = [entry.name for entry in os.scandir(directory) if entry.is_dir()]
    except OSError:
        return
    for name in subDirectoryNames:
        # A directory that is not a valid package name holds no package: '.git', 'build-x', a template directory like 'C++'
        if not Util.IsValidPackageName(name):
            continue
        path = IOUtil.Join(directory, name)
        if path in skipDirectories:
            continue
        genFile = IOUtil.Join(path, genFileName)
        if IOUtil.IsFile(genFile):
            rGenFiles.append(genFile)
        _AddGenFilesBelow(rGenFiles, path, genFileName, skipDirectories)


def FindGenFilesInPackageLocations(packageLocations: list[ToolConfigPackageLocation], genFileName: str, ignoreDirectories: list[str]) -> list[str]:
    """The gen file of every package the package loader can find in the package locations, whatever the scan method of a location is (the
    loader finds a package it is asked for by name at any depth).
    The directory rules are the ones of the loader (PackageLocationCache): a directory whose name is not a valid package name is not entered,
    a blacklisted directory of a location is not entered, and a package location inside another one is walked once, as the location it is.
    """
    rootLocationPaths = {location.ResolvedPath for location in packageLocations}
    genFiles: list[str] = []
    for location in packageLocations:
        skipDirectories = rootLocationPaths.union(ignoreDirectories, (entry.AbsoluteDirPath for entry in location.Blacklist))
        _AddGenFilesBelow(genFiles, location.ResolvedPath, genFileName, skipDirectories)
    return genFiles


def FindGenFiles(config: Config, startDirectory: str, recursive: bool, additionalDirs: list[str] | None = None) -> list[str]:
    """The gen files the scan checks, sorted and each one once.
    - The default: the gen file of the package in startDirectory, with 'recursive' every gen file below startDirectory. It is the list the
      other checks load their packages from (Main.DoGetFiles).
    - A sdk build ('-t sdk'): the gen file of every package in the package locations of the project.
    """
    toolMiniConfig = GetMinimalConfig(config.ToolConfig)
    genFiles = MainFlow.DoGetFiles(config, toolMiniConfig, startDirectory, recursive, additionalDirs)
    if config.IsSDKBuild:
        packageConfiguration = config.ToolConfig.PackageConfiguration.get(config.Type)
        packageLocations = packageConfiguration.Locations if packageConfiguration is not None else []
        genFiles += FindGenFilesInPackageLocations(packageLocations, config.GenFileName, toolMiniConfig.IgnoreDirectories)
    return sorted(set(genFiles))


def _CheckProject(projectContext: ToolConfigProjectContext, currentVersion: int) -> None:
    """Stop before anything is listed or repaired when the gen files of the project can not refer to its schema: a repair would point every
    gen file at nothing.
    """
    location = projectContext.GenFileSchemaLocation
    if location.IsUrl:
        return
    try:
        GenFileSchema.GetExpectedReference(location, projectContext.Location.ResolvedPath, currentVersion)
    except UsageErrorException as ex:
        raise UsageErrorException(f"Project '{projectContext.ProjectName}': {ex}") from ex
    schemaFile = IOUtil.Join(location.Value, f"{currentVersion}/{GenFileSchema.SchemaFileName}")
    if not IOUtil.IsFile(schemaFile):
        raise UsageErrorException(
            f"Project '{projectContext.ProjectName}': the gen file schema directory '{location.Value}' has no schema version {currentVersion}, "
            f"the file '{schemaFile}' was not found"
        )


def _Reason(ex: OSError) -> str:
    """Why the operating system refused, without the file name (the message has it)"""
    return ex.strerror if ex.strerror is not None else str(ex)


def _TryRepair(log: Log, genFile: str, content: bytes, expectedReference: str, disableWrite: bool) -> bool:
    """Write the gen file with the expected reference, returns true if the file was written"""
    try:
        newContent = GenFileSchemaReference.Replace(content, expectedReference)
    except GenFileSchemaReferenceError as ex:
        log.DoPrint(f"Can not repair '{genFile}': {ex}")
        return False
    if disableWrite:
        log.DoPrint(f"Would repair '{genFile}'")
        return False
    try:
        IOUtil.WriteBinaryFile(genFile, newContent)
    except OSError as ex:
        log.DoPrint(f"Can not repair '{genFile}': the file can not be written ({_Reason(ex)})")
        return False
    log.DoPrint(f"Repaired '{genFile}'")
    return True


def Scan(
    log: Log,
    genFiles: list[str],
    projectContexts: list[ToolConfigProjectContext],
    repairEnabled: bool,
    disableWrite: bool,
    currentVersion: int = GenFileSchema.CurrentVersion,
) -> ScanGenFilesCounts:
    """Check the schema reference of the gen files and list the wrong ones.
    :param genFiles: the absolute paths of the gen files, written the way the tool writes a path.
    :param repairEnabled: write the expected reference into a gen file that has a wrong one.
    :param disableWrite: a dry run, a repair only tells which files it would write.
    """
    log.LogPrint("Running gen file scan")

    # The project of each gen file. The projects are checked first, so nothing is repaired when one of them is set up wrong.
    genFileProjects = [ToolConfigPackageProjectContextUtil.TryFindToProjectContext(projectContexts, IOUtil.GetDirectoryName(genFile)) for genFile in genFiles]
    for projectContext in projectContexts:
        if projectContext in genFileProjects:
            _CheckProject(projectContext, currentVersion)

    counts = ScanGenFilesCounts()
    for genFile, projectContext in zip(genFiles, genFileProjects, strict=True):
        try:
            content = IOUtil.ReadBinaryFile(genFile)
        except OSError as ex:
            log.DoPrint(f"Can not check the schema reference of '{genFile}': the file can not be read ({_Reason(ex)})")
            counts.NotChecked += 1
            continue
        found = GenFileSchemaReference.Inspect(content)
        if found.Status == ReferenceStatus.Missing:
            log.DoPrint(f"No schema reference in '{genFile}'")
            counts.NotChecked += 1
            continue
        if found.Value is None:
            log.DoPrint(f"Can not check the schema reference of '{genFile}': {found.Reason}")
            counts.NotChecked += 1
            continue
        if projectContext is None:
            log.DoPrint(f"Can not check the schema reference of '{genFile}': the file is not inside a project")
            counts.NotChecked += 1
            continue

        counts.Checked += 1
        genFileDirectory = IOUtil.GetDirectoryName(genFile)
        referenceClass = GenFileSchema.ClassifyReference(found.Value, projectContext.GenFileSchemaLocation, genFileDirectory, currentVersion)
        if referenceClass == ReferenceClass.Current:
            continue
        if referenceClass == ReferenceClass.Older:
            log.LogPrintVerbose(2, f"Older schema version: '{genFile}' refers to '{found.Value}'")
            continue

        counts.Wrong += 1
        expectedReference = GenFileSchema.GetExpectedReference(projectContext.GenFileSchemaLocation, genFileDirectory, currentVersion)
        log.DoPrint(f"Wrong schema reference: '{genFile}' expected '{expectedReference}'")
        log.DoPrint(f"- Was '{found.Value}'")
        if repairEnabled and _TryRepair(log, genFile, content, expectedReference, disableWrite):
            counts.Repaired += 1

    log.DoPrint(f"Gen file scan: {counts.Checked} checked, {counts.Wrong} wrong, {counts.Repaired} repaired, {counts.NotChecked} not checked")
    if counts.Wrong > 0 and not repairEnabled:
        log.DoPrint(
            "Use '--repair' to set the wrong schema references, nothing else in a gen file is changed (add '--dryRun' to see which files it would write)"
        )
    return counts
