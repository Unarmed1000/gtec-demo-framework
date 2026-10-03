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

from FslBuildGen import IOUtil, ToolSharedValues
from FslBuildGen.BuildContent.BasicContentProcessor import BasicContentProcessor
from FslBuildGen.BuildContent.ContentBuildResult import ContentBuildResult
from FslBuildGen.BuildContent.ContentDependencyCache import ContentDependencyCache, CreateOutputDependencies

# from FslBuildGen.BuildContent.ContentProcessor import ContentProcessor
from FslBuildGen.BuildContent.ContentRootRecord import ContentRootRecord
from FslBuildGen.BuildContent.MakeDependencyFile import BuildMakeDependencyFile
from FslBuildGen.BuildContent.PathRecord import PathRecord
from FslBuildGen.BuildContent.PathVariables import PathVariables
from FslBuildGen.BuildContent.Processor.ContentBuildCommandFile import ContentBuildCommandFile

# from FslBuildGen.BuildContent.Processor.Commands import Command
# from FslBuildGen.BuildContent.Processor.Commands import CommandContentBuildSync
from FslBuildGen.BuildContent.Processor.SourceContent import SourceContent
from FslBuildGen.BuildContent.Sync import BuildState
from FslBuildGen.BuildContent.Sync.Content import Content
from FslBuildGen.BuildContent.ToolFinder import ToolFinder
from FslBuildGen.Log import Log
from FslBuildGen.PackagePath import PackagePath
from FslBuildGen.ToolConfig import ToolConfig, ToolConfigContentBuilderConfiguration

# from FslBuildGen.BuildContent.VulkanContentProcessor import VulkanContentProcessor


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
    def __init__(self, log: Log, toolConfig: ToolConfig, features: Features, toolFinder: ToolFinder) -> None:
        super().__init__()

        contentProcessors: list[BasicContentProcessor] = []
        #        contentProcessors = [VulkanContentProcessor()]
        if toolConfig.ContentBuilderConfiguration is not None:
            contentProcessors += self.__AddBasicContentProcessors(log, toolFinder, toolConfig.ContentBuilderConfiguration)
        self.__ContentProcessors = self.__FilterProcessorsBasedOnFeatures(contentProcessors, features)

    def TryFindContentProcessor(self, contentFile: PathRecord) -> BasicContentProcessor | None:
        processors = self.__FindProcessors(self.__ContentProcessors, contentFile.ResolvedPath)

        if len(processors) > 1:
            contentProcessorNames = [processor.Name for processor in processors]
            raise Exception(f"Multiple content processors '{contentProcessorNames}' available for '{contentFile.ResolvedPath}'")

        return processors[0] if len(processors) == 1 else None

    def __AddBasicContentProcessors(
        self, log: Log, toolFinder: ToolFinder, contentBuilderConfiguration: ToolConfigContentBuilderConfiguration
    ) -> list[BasicContentProcessor]:
        """Add basic content builders from the tool config XML file"""
        contentBuilders: list[BasicContentProcessor] = []
        for contentBuilder in contentBuilderConfiguration.ContentBuilders:
            contentBuilders.append(BasicContentProcessor(log, toolFinder, contentBuilder))
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
    ) -> None:
        """dependencyFile: a make style dependency file to write for the build system, listing the files the outputs depend on beyond Content.bld"""
        super().__init__()
        self.Result = ContentBuildResult()

        configPathVariables = PathVariables(toolConfig, packageBuildPath, contentBuildPath, contentOutputPath)
        commandFilename = IOUtil.Join(contentBuildPath, ToolSharedValues.CONTENT_BUILD_FILE_NAME)
        commandFile = ContentBuildCommandFile(log, commandFilename, configPathVariables)

        # We don't include the files at "Content" (contentOutputPath)
        sourceContent = SourceContent(log, contentOutputPath, contentBuildPath, commandFile, False, commandFilename)

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

        dependencyCache = ContentDependencyCache(log, absoluteDependencyCacheFileName)

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
        for contentFile in srcContent.Files:
            processor = contentProcessorManager.TryFindContentProcessor(contentFile)
            if processor is None:
                # Normal for files that are only built for other features (like GLES shaders in a Vulkan build)
                log.LogPrintVerbose(1, f"No content processor for '{contentFile.ResolvedPath}' with the active features, skipped")
                self.Result.NoProcessor += 1
            else:
                # Query the processor for the output filename
                outputFileName = processor.GetOutputFileName(log, contentOutputPath, contentFile)
                outputFileRecord = PathRecord(log, dstRoot, outputFileName[len(dstRoot.ResolvedPath) + 1 :])

                # Query the sync state of the content file
                syncStateFileName = self.__GetSyncStateFileName(contentBuildPath, contentFile.RelativePath)
                contentState = syncState.TryGetFileStateByFileName(syncStateFileName)
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

                if buildResource:
                    try:
                        dependencies = processor.Process(log, configDisableWrite, contentBuildPath, contentOutputPath, contentFile)
                        if not configDisableWrite:
                            outputName = self.__GetSyncStateFileName(contentOutputPath, outputFileName)
                            dependencyCache.Set(outputName, CreateOutputDependencies(contentFile.ResolvedPath, dependencies))
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


def GetContentProcessorManager(log: Log, toolConfig: ToolConfig, featureList: list[str]) -> ContentProcessorManager:
    toolFinder = ToolFinder(log)
    features = Features(log, featureList)
    return ContentProcessorManager(log, toolConfig, features, toolFinder)


def GetContentOutputPath(packagePath: PackagePath) -> str:
    currentPath = packagePath.AbsoluteDirPath
    return IOUtil.Join(currentPath, ToolSharedValues.CONTENT_FOLDER_NAME)


def Build(
    log: Log,
    configBuildDir: str,
    configDisableWrite: bool,
    toolConfig: ToolConfig,
    packagePath: PackagePath,
    featureList: list[str],
    outputPath: str | None = None,
    dependencyFile: str | None = None,
) -> None:
    currentPath = packagePath.AbsoluteDirPath
    contentBuildDir = ToolSharedValues.CONTENT_BUILD_FOLDER_NAME
    contentBuildPath = IOUtil.Join(currentPath, contentBuildDir)

    contentOutputPath = GetContentOutputPath(packagePath) if outputPath is None else outputPath
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

    contentProcessorManager = GetContentProcessorManager(log, toolConfig, featureList)
    builder = Builder(
        log, configDisableWrite, toolConfig, packageBuildPath, contentBuildPath, contentOutputPath, contentProcessorManager, outputRequired, dependencyFile
    )
    # Always report what was done, a build log should show that the content was built
    log.DoPrint(builder.Result.GetSummary(currentPath, contentOutputPath))
