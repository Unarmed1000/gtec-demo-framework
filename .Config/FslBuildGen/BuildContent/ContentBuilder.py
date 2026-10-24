#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright (c) 2016 Freescale Semiconductor, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    * Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#
#    * Redistributions in binary form must reproduce the above copyright notice,
#      this list of conditions and the following disclaimer in the documentation
#      and/or other materials provided with the distribution.
#
#    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
#      its contributors may be used to endorse or promote products derived from
#      this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ****************************************************************************************************************************************************

import shutil
import time
from typing import Any

from FslBuildGen import ContentBuilderPlatforms, IOUtil, ToolSharedValues
from FslBuildGen.BuildContent import ContentBuildSteps, ContentPlatformFilter, ContentProfileList
from FslBuildGen.BuildContent.BasicContentProcessor import BasicContentProcessor
from FslBuildGen.BuildContent.ContentBuildResult import ContentBuildResult
from FslBuildGen.BuildContent.ContentBuildRules import ContentBuildRules
from FslBuildGen.BuildContent.ContentBuildSteps import ContentBuildStep
from FslBuildGen.BuildContent.ContentDependencyCache import ContentDependencyCache, CreateOutputDependencies
from FslBuildGen.BuildContent.ContentProfile import ResolvedContentProfile

# from FslBuildGen.BuildContent.ContentProcessor import ContentProcessor
from FslBuildGen.BuildContent.ContentRootRecord import ContentRootRecord
from FslBuildGen.BuildContent.MakeDependencyFile import BuildMakeDependencyFile
from FslBuildGen.BuildContent.PathRecord import PathRecord
from FslBuildGen.BuildContent.PathVariables import PathVariables
from FslBuildGen.BuildContent.Processor.ContentBuildCommandFile import ContentBuildCommandFile
from FslBuildGen.BuildContent.Processor.ListedSourceContent import ListedSourceContent

# from FslBuildGen.BuildContent.Processor.Commands import Command
# from FslBuildGen.BuildContent.Processor.Commands import CommandContentBuildSync
from FslBuildGen.BuildContent.Processor.SourceContent import SourceContent
from FslBuildGen.BuildContent.Sync import BuildState
from FslBuildGen.BuildContent.Sync.Content import Content
from FslBuildGen.BuildContent.ToolFinder import ToolFinder
from FslBuildGen.Log import Log
from FslBuildGen.PackagePath import PackagePath
from FslBuildGen.ToolConfig import ToolConfig, ToolConfigContentBuilderConfiguration


def GetContentOutputContentRootRecord(log: Log, contentOutputPath: str) -> ContentRootRecord:
    return ContentRootRecord(log, contentOutputPath)


def GetContentSyncOutputFilename(log: Log, dstRoot: ContentRootRecord, contentFile: PathRecord) -> PathRecord:
    return PathRecord(log, dstRoot, contentFile.RelativePath)


class Features:
    def __init__(self, log: Log, features: list[str]) -> None:
        super().__init__()
        featureIds = [feature.lower() for feature in features]
        self.Features = features
        self.FeaturesIds: list[str] = featureIds
        self.UseVulkan = "vulkan" in featureIds
        self.UseOpenGLES = "opengles2" in features or "opengles3" in featureIds or "opengles3.1" in featureIds


class ContentProcessorManager:
    def __init__(self, log: Log, toolConfig: ToolConfig, features: Features, toolFinder: ToolFinder, platformName: str | None = None) -> None:
        """platformName: the platform the content is built for, None gives the builders of every platform"""
        super().__init__()

        contentProcessors: list[BasicContentProcessor] = []
        if toolConfig.ContentBuilderConfiguration is not None:
            contentProcessors += self.__AddBasicContentProcessors(log, toolFinder, toolConfig.ContentBuilderConfiguration, platformName)
        # Every builder of the tool config, a build rule can name one that is not for this app
        self.__KnownNames = [contentProcessor.Name for contentProcessor in contentProcessors]
        contentProcessors = [entry for entry in contentProcessors if ContentBuilderPlatforms.IsFor(entry.PlatformNames, platformName)]
        self.__ContentProcessors = self.__FilterProcessorsBasedOnFeatures(contentProcessors, features)

    def TryFindContentProcessor(self, contentFile: PathRecord) -> BasicContentProcessor | None:
        processors = self.__FindProcessors(self.__ContentProcessors, contentFile.ResolvedPath)

        if len(processors) > 1:
            contentProcessorNames = [processor.Name for processor in processors]
            raise Exception(f"Multiple content processors '{contentProcessorNames}' available for '{contentFile.ResolvedPath}'")

        return processors[0] if len(processors) == 1 else None

    def TryFindContentProcessorByName(self, name: str) -> BasicContentProcessor | None:
        """The builder of that name if it is for this app (its features, its platform)"""
        for processor in self.__ContentProcessors:
            if processor.Name == name:
                return processor
        return None

    def IsKnownContentProcessor(self, name: str) -> bool:
        return name in self.__KnownNames

    def GetKnownContentProcessorNames(self) -> list[str]:
        return list(self.__KnownNames)

    def __AddBasicContentProcessors(
        self, log: Log, toolFinder: ToolFinder, contentBuilderConfiguration: ToolConfigContentBuilderConfiguration, platformName: str | None
    ) -> list[BasicContentProcessor]:
        """Add basic content builders from the tool config XML file"""
        contentBuilders: list[BasicContentProcessor] = []
        for contentBuilder in contentBuilderConfiguration.ContentBuilders:
            contentBuilders.append(BasicContentProcessor(log, toolFinder, contentBuilder, platformName))
        return contentBuilders

    def __FilterProcessorsBasedOnFeatures(self, contentProcessors: list[BasicContentProcessor], features: Features) -> list[BasicContentProcessor]:
        return [contentProcessor for contentProcessor in contentProcessors if contentProcessor.CheckFeatureRequirements(features.FeaturesIds)]

    def __FindProcessors(self, contentProcessors: list[BasicContentProcessor], contentFile: str) -> list[BasicContentProcessor]:
        extension = IOUtil.GetFileNameExtension(contentFile)[1:].lower()
        return [contentProcessor for contentProcessor in contentProcessors if extension in contentProcessor.FileExtensionSet]


class Builder:
    def __init__(
        self,
        log: Log,
        configDisableWrite: bool,
        toolConfig: ToolConfig,
        packageBuildPath: str,
        contentBuildPath: str,
        contentOutputPath: str,
        contentProcessorManager: ContentProcessorManager,
        outputRequired: bool,
        dependencyFile: str | None = None,
        platformName: str | None = None,
        listedContent: ListedSourceContent | None = None,
    ) -> None:
        """dependencyFile: a make style dependency file to write for the build system, listing the files the outputs depend on beyond Content.bld
        platformName: the platform the content is built for, needed when the command file lists files that are not for every platform
        listedContent: what to build and to copy, when it is not what the command file and the directory of contentBuildPath hold: the
                       content of an app with the files of the libraries it uses (ContentDelivery)
        """
        super().__init__()
        self.Result = ContentBuildResult()

        # A file that is listed is built where it comes from, like a file of contentBuildPath is built there
        self.__BuildInSourceRoot = listedContent is not None
        self.__ListedBuildSteps = listedContent.BuildSteps if listedContent is not None else None
        self.__BuildRules: ContentBuildRules | None = None
        # What each file to build is, by its resolved path, when the command file is read here. A listed step has its own
        self.__Profiles: dict[str, ResolvedContentProfile] = {}
        sourceContent: SourceContent | ListedSourceContent = (
            listedContent
            if listedContent is not None
            else self.__ReadSourceContent(log, toolConfig, packageBuildPath, contentBuildPath, contentOutputPath, platformName)
        )

        absoluteCacheFileName = IOUtil.Join(packageBuildPath, "_ContentBuildCache.fsl")
        absoluteOutputCacheFileName = IOUtil.Join(packageBuildPath, "_ContentBuildCacheOutput.fsl")
        absoluteDependencyCacheFileName = IOUtil.Join(packageBuildPath, "_ContentBuildCacheDependencies.fsl")

        srcsSyncState = BuildState.GenerateSyncState(log, absoluteCacheFileName, sourceContent.AllContentSource, True)
        outputSyncState = BuildState.GenerateOutputSyncState(log, absoluteOutputCacheFileName, contentOutputPath, True)

        if sourceContent.IsEmpty:
            if outputRequired:
                raise Exception(f"No content files found at '{contentBuildPath}', but the build expects content in '{contentOutputPath}'")
            log.LogPrint("No files found")
            return

        if not configDisableWrite:
            IOUtil.SafeMakeDirs(contentOutputPath)

        # The clock tells the cache which dependencies were written just before their state is recorded
        dependencyCache = ContentDependencyCache(log, absoluteDependencyCacheFileName, time.time_ns)

        self.__ProcessSyncFiles(log, contentBuildPath, contentOutputPath, sourceContent.ContentSource, srcsSyncState, outputSyncState)
        try:
            self.__ProcessContentFiles(
                log,
                configDisableWrite,
                contentBuildPath,
                contentOutputPath,
                contentProcessorManager,
                sourceContent.ContentBuildSource,
                srcsSyncState,
                outputSyncState,
                dependencyCache,
            )
        except:
            if not configDisableWrite:
                dependencyCache.Save(False)
            raise
        srcsSyncState.Save()
        outputSyncState.Save()

        if not configDisableWrite:
            dependencyCache.Save(True)
            missingOutputFiles = self.Result.GetMissingOutputFiles()
            if len(missingOutputFiles) > 0:
                raise Exception("The content build did not create: {}".format(", ".join(f"'{entry}'" for entry in missingOutputFiles)))
            if dependencyFile is not None:
                IOUtil.WriteFileUTF8IfChanged(dependencyFile, BuildMakeDependencyFile(sorted(self.Result.OutputFiles), dependencyCache.GetAllDependencies()))

    def __ReadSourceContent(
        self, log: Log, toolConfig: ToolConfig, packageBuildPath: str, contentBuildPath: str, contentOutputPath: str, platformName: str | None
    ) -> SourceContent:
        configPathVariables = PathVariables(toolConfig, packageBuildPath, contentBuildPath, contentOutputPath)
        commandFilename = IOUtil.Join(contentBuildPath, ToolSharedValues.CONTENT_BUILD_FILE_NAME)
        commandFile = ContentBuildCommandFile(log, commandFilename, configPathVariables)

        # We don't include the files at "Content" (contentOutputPath)
        sourceContent = SourceContent(log, contentOutputPath, contentBuildPath, commandFile, False, commandFilename)
        # As the resolver does: checked against every file the command file is about, whatever platform it is for
        records = sourceContent.ContentBuildSource.Files + sourceContent.ContentSource.Files
        profiles = ContentProfileList.Resolve(commandFile.ProfileList, dict.fromkeys(record.RelativePath for record in records))
        self.__Profiles = {record.ResolvedPath: profiles[record.RelativePath] for record in records}
        ContentPlatformFilter.RemoveBuildFilesNotForPlatform(sourceContent, commandFile.PlatformList, contentBuildPath, platformName)
        self.__BuildRules = commandFile.BuildRules
        return sourceContent

    def __GetSyncStateFileName(self, contentBuildPath: str, contentFile: str) -> str:
        if contentFile.startswith(contentBuildPath):
            contentFile = contentFile[len(contentBuildPath) :]
            if contentFile.startswith("/"):
                contentFile = contentFile[1:]
        return contentFile

    def __ProcessSyncFiles(
        self,
        log: Log,
        contentBuildPath: str,
        contentOutputPath: str,
        srcContent: Content,
        syncState: BuildState.SyncState,
        outputSyncState: BuildState.SyncState,
    ) -> None:
        dstRoot = GetContentOutputContentRootRecord(log, contentOutputPath)
        for contentFile in srcContent.Files:
            # Generate the output file record
            outputFileRecord = GetContentSyncOutputFilename(log, dstRoot, contentFile)
            outputFileName = contentFile.RelativePath

            ## Query the sync state of the content file
            syncStateFileName = self.__GetSyncStateFileName(contentFile.SourceRoot.ResolvedPath, contentFile.RelativePath)
            contentState = syncState.TryGetFileStateByFileName(syncStateFileName)
            buildResource = contentState is None or contentState.CacheState != BuildState.CacheState.Unmodified
            if not buildResource:
                # It was unmodified, so we need to examine the state of the output file to
                # determine if its safe to skip the building
                syncStateOutputFileName = self.__GetSyncStateFileName(contentOutputPath, outputFileName)
                outputContentState = outputSyncState.TryGetFileStateByFileName(syncStateOutputFileName)
                buildResource = not outputContentState or outputContentState.CacheState != BuildState.CacheState.Unmodified

            if buildResource:
                try:
                    log.LogPrintVerbose(2, f"Copying '{contentFile.ResolvedPath}' to '{outputFileRecord.ResolvedPath}'")
                    dstDirPath = IOUtil.GetDirectoryName(outputFileRecord.ResolvedPath)
                    IOUtil.SafeMakeDirs(dstDirPath)
                    shutil.copy(contentFile.ResolvedPath, outputFileRecord.ResolvedPath)
                except:
                    # Save if a exception occured to prevent reprocessing the working files
                    outputSyncState.Save()
                    syncState.Save()
                    raise

                # Add a entry for the output file
                outputFileState = outputSyncState.BuildContentState(log, outputFileRecord, True, True)
                outputSyncState.Add(outputFileState)
                self.Result.Synced += 1
            else:
                self.Result.UpToDate += 1
            self.Result.OutputFiles.append(outputFileRecord.ResolvedPath)

    def __ProcessContentFiles(
        self,
        log: Log,
        configDisableWrite: bool,
        contentBuildPath: str,
        contentOutputPath: str,
        contentProcessorManager: ContentProcessorManager,
        srcContent: Content,
        syncState: BuildState.SyncState,
        outputSyncState: BuildState.SyncState,
        dependencyCache: ContentDependencyCache,
    ) -> None:
        dstRoot = ContentRootRecord(log, contentOutputPath)
        # What there is to do is found before anything is built, so a tool that is missing is told before the first file and not
        # in the middle of a build
        work: list[tuple[PathRecord, ContentBuildStep, PathRecord, BuildState.ContentState | None, bool]] = []
        for contentFile in srcContent.Files:
            steps = self.__GetBuildSteps(log, contentProcessorManager, contentFile)
            if len(steps) <= 0:
                # Normal for files that are only built for other features (like GLES shaders in a Vulkan build)
                log.LogPrintVerbose(1, f"No content processor for '{contentFile.ResolvedPath}' with the active features, skipped")
                self.Result.NoProcessor += 1
                continue

            # Query the sync state of the content file
            syncStateFileName = self.__GetSyncStateFileName(contentBuildPath, contentFile.RelativePath)
            contentState = syncState.TryGetFileStateByFileName(syncStateFileName)
            for step in steps:
                outputFileRecord = PathRecord(log, dstRoot, step.OutputRelativePath)
                outputFileName = outputFileRecord.ResolvedPath
                buildResource = contentState is None or contentState.CacheState != BuildState.CacheState.Unmodified
                if not buildResource:
                    # It was unmodified, so we need to examine the state of the output file to
                    # determine if its safe to skip the building
                    syncStateOutputFileName = self.__GetSyncStateFileName(contentOutputPath, outputFileName)
                    outputContentState = outputSyncState.TryGetFileStateByFileName(syncStateOutputFileName)
                    buildResource = (
                        outputContentState is None
                        or outputContentState.CacheState != BuildState.CacheState.Unmodified
                        or (contentState is None or contentState.Checksum != outputContentState.TagChecksum)
                    )
                    if not buildResource:
                        # The files the content file includes can change while the content file does not
                        buildReason = dependencyCache.TryGetBuildReason(syncStateOutputFileName)
                        if buildReason is not None:
                            log.LogPrintVerbose(1, f"Building '{contentFile.ResolvedPath}' as {buildReason}")
                            buildResource = True
                work.append((contentFile, step, outputFileRecord, contentState, buildResource))

        self.__CheckTools([step.Processor for _contentFile, step, _outputFileRecord, _contentState, buildResource in work if buildResource])

        for contentFile, step, outputFileRecord, contentState, buildResource in work:
            outputFileName = outputFileRecord.ResolvedPath
            if buildResource:
                try:
                    workPath = contentFile.SourceRoot.ResolvedPath if self.__BuildInSourceRoot else contentBuildPath
                    # The output of the step, also for the builder of the extension: the app path of a listed step can differ from the
                    # path the builder names (a library with a content namespace). A build rule gives its parameters too
                    dependencies = step.Processor.Process(
                        log, configDisableWrite, workPath, contentOutputPath, contentFile, outputFileName, step.ToolArguments, step.Profile
                    )
                    if not configDisableWrite:
                        outputName = self.__GetSyncStateFileName(contentOutputPath, outputFileName)
                        dependencyCache.Set(outputName, CreateOutputDependencies(contentFile.ResolvedPath, dependencies, time.time_ns()))
                except:
                    # Save if a exception occured to prevent reprocessing the working files, but we invalidate
                    outputSyncState.Save()
                    syncState.Save()
                    raise

                # Add a entry for the output file
                outputFileState = outputSyncState.BuildContentState(log, outputFileRecord, True, True)
                # Tag it with the source file checksum so we have another way to detect changes
                if contentState is not None:
                    outputFileState.TagChecksum = contentState.Checksum
                outputSyncState.Add(outputFileState)
                self.Result.Built += 1
            else:
                self.Result.UpToDate += 1
            self.Result.OutputFiles.append(outputFileRecord.ResolvedPath)

    def __GetBuildSteps(self, log: Log, contentProcessorManager: ContentProcessorManager, contentFile: PathRecord) -> list[ContentBuildStep]:
        """What builds the file: the steps it was listed with, or what the build rules of the directory and the builders say"""
        listedSteps = self.__ListedBuildSteps.get(contentFile.ResolvedPath) if self.__ListedBuildSteps is not None else None
        if listedSteps is None:
            steps = ContentBuildSteps.FindBuildSteps(log, contentProcessorManager, contentFile, self.__BuildRules)
            for step in steps:
                step.Profile = self.__Profiles.get(contentFile.ResolvedPath)
            return steps
        steps: list[ContentBuildStep] = []
        for listedStep in listedSteps:
            processor = (
                contentProcessorManager.TryFindContentProcessor(contentFile)
                if listedStep.BuilderName is None
                else contentProcessorManager.TryFindContentProcessorByName(listedStep.BuilderName)
            )
            if processor is not None:
                steps.append(ContentBuildStep(processor, listedStep.OutputRelativePath, listedStep.ToolArguments, listedStep.BuilderName, listedStep.Profile))
        return steps

    def __CheckTools(self, processors: list[Any]) -> None:
        checked: list[Any] = []
        for processor in processors:
            if not any(processor is entry for entry in checked):
                checked.append(processor)
                checkTool = getattr(processor, "CheckTool", None)
                if checkTool is not None:
                    checkTool()


def GetContentProcessorManager(log: Log, toolConfig: ToolConfig, featureList: list[str], platformName: str | None = None) -> ContentProcessorManager:
    toolFinder = ToolFinder(log)
    features = Features(log, featureList)
    return ContentProcessorManager(log, toolConfig, features, toolFinder, platformName)


def GetContentOutputPath(packagePath: PackagePath, toolConfig: ToolConfig) -> str:
    """The content directory of the package itself, where what it builds is put when nothing else is asked for"""
    currentPath = packagePath.AbsoluteDirPath
    return IOUtil.Join(currentPath, toolConfig.ContentDirectory.Name)


def Build(
    log: Log,
    configBuildDir: str,
    configDisableWrite: bool,
    toolConfig: ToolConfig,
    packagePath: PackagePath,
    featureList: list[str],
    outputPath: str | None = None,
    dependencyFile: str | None = None,
    platformName: str | None = None,
) -> None:
    currentPath = packagePath.AbsoluteDirPath
    contentBuildDir = toolConfig.ContentDirectory.BuildName
    contentBuildPath = IOUtil.Join(currentPath, contentBuildDir)

    contentOutputPath = GetContentOutputPath(packagePath, toolConfig) if outputPath is None else outputPath
    # A explicit output path means a build system asked for the content (and expects the output files it knows about)
    outputRequired = outputPath is not None

    if not IOUtil.IsDirectory(contentBuildPath):
        if outputRequired:
            raise Exception(f"No '{contentBuildDir}' directory present at '{currentPath}', but the build expects content in '{contentOutputPath}'")
        log.LogPrintVerbose(1, f"No '{contentBuildDir}' directory present at '{currentPath}' so there is no content to process.")
        return

    packageBuildPath = IOUtil.Join(currentPath, configBuildDir)
    if not configDisableWrite:
        IOUtil.SafeMakeDirs(packageBuildPath)

    contentProcessorManager = GetContentProcessorManager(log, toolConfig, featureList, platformName)
    builder = Builder(
        log,
        configDisableWrite,
        toolConfig,
        packageBuildPath,
        contentBuildPath,
        contentOutputPath,
        contentProcessorManager,
        outputRequired,
        dependencyFile,
        platformName,
    )
    # Always report what was done, a build log should show that the content was built
    log.DoPrint(builder.Result.GetSummary(currentPath, contentOutputPath))
