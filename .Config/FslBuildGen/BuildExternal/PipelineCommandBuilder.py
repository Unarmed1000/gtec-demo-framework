#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright 2017 NXP
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
#    * Neither the name of the NXP. nor the names of
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

# from typing import Union
import shlex
import shutil
import time

# from FslBuildGen.Xml.XmlExperimentalRecipe import XmlRecipePipelineBasicCommand
from FslBuildGen import IOUtil
from FslBuildGen.AndroidUtil import AndroidUtil
from FslBuildGen.BuildExternal.PackageExperimentalRecipe import PackageExperimentalRecipe

# from FslBuildGen.BuildExternal.PipelineBasicCommand import PipelineBasicCommand
from FslBuildGen.BuildExternal.PipelineCommand import PipelineCommand
from FslBuildGen.BuildExternal.PipelineInfo import PipelineInfo

# from FslBuildGen.BuildExternal.PipelineJoinCommand import PipelineJoinCommand
from FslBuildGen.BuildExternal.PipelineTasks import PipelineTasks
from FslBuildGen.CMakeUtil import CMakeUtil

# from FslBuildGen.BuildExternal.RecipePathBuilder import RecipePathBuilder
# from FslBuildGen.BuildExternal.Tasks import GitApplyTask
from FslBuildGen.Context.GeneratorContext import GeneratorContext
from FslBuildGen.DataTypes import BuildRecipePipelineCommand, BuildVariantConfig, CMakeTargetType, PackageType
from FslBuildGen.Generator.Report.ParsedFormatString import ParsedFormatString
from FslBuildGen.Generator.Report.StringVariableDict import StringVariableDict
from FslBuildGen.Log import Log
from FslBuildGen.PackageConfig import PlatformNameString
from FslBuildGen.Packages.Package import Package
from FslBuildGen.PackageToolFinder import PackageToolFinder
from FslBuildGen.Xml.XmlExperimentalRecipe import (
    XmlRecipePipelineBuildCommand,
    XmlRecipePipelineCommand,
    XmlRecipePipelineCommandCMakeBuild,
    XmlRecipePipelineCommandCombine,
    XmlRecipePipelineCommandCopy,
    XmlRecipePipelineCommandUnpack,
    XmlRecipePipelineFetchCommand,
    XmlRecipePipelineFetchCommandDownload,
    XmlRecipePipelineFetchCommandGitClone,
    XmlRecipePipelineFetchCommandSource,
)


class PipelineCommandFetch(PipelineCommand):
    def __init__(self, log: Log, sourceCommand: XmlRecipePipelineFetchCommand, pipelineInfo: PipelineInfo) -> None:
        super().__init__(log, sourceCommand, pipelineInfo)
        self.Hash = "NotDefined" if sourceCommand is None or sourceCommand.Hash is None else sourceCommand.Hash

    @staticmethod
    def GenerateFileHash(filename: str) -> str:
        return IOUtil.HashFile(filename)


class PipelineCommandNOP(PipelineCommandFetch):
    def __init__(self, log: Log, sourceCommand: XmlRecipePipelineFetchCommand, pipelineInfo: PipelineInfo) -> None:
        super().__init__(log, sourceCommand, pipelineInfo)
        self.AutoCreateDstDirectory = False


class PipelineCommandDownload(PipelineCommandFetch):
    def __init__(self, log: Log, sourceCommand: XmlRecipePipelineFetchCommandDownload, pipelineInfo: PipelineInfo) -> None:
        super().__init__(log, sourceCommand, pipelineInfo)
        if pipelineInfo.Tasks.TaskDownload is None:
            raise Exception(f"The '{sourceCommand.CommandName}' operation has not been enabled for this platform")
        self.Task = pipelineInfo.Tasks.TaskDownload
        self.__SourceCommand = sourceCommand

    def DoExecute(self) -> None:
        # resolved before the try so the error handler below can always reference it
        targetFilename = PipelineCommandDownload.GetTargetFilename(self.__SourceCommand)
        archiveFilePath = IOUtil.Join(self.Info.DstRootPath, targetFilename)
        try:
            if not self.Info.AllowDownloads and not PipelineCommandDownload.IsValidCacheFile(archiveFilePath, self.__SourceCommand):
                raise Exception(
                    f"Could not download {self.__SourceCommand.URL} to {archiveFilePath} as downloads have been disabled. Enable downloads or download it manually."
                )

            self.Task.DownloadFromUrl(self.__SourceCommand.URL, archiveFilePath)
            # Generate file hash
            filehash = PipelineCommandFetch.GenerateFileHash(archiveFilePath)
            if self.__SourceCommand.Hash is None:
                if self.Log.Verbosity >= 1:
                    self.Log.LogPrintWarning(f"No hash value defined for file {archiveFilePath} which has a hash value of {filehash}")
            elif filehash != self.__SourceCommand.Hash:
                raise Exception(
                    f"The downloaded file {archiveFilePath} has a hash of {filehash} which did not match the expected value of {self.__SourceCommand.Hash}"
                )
            elif self.Log.Verbosity >= 2:
                self.LogPrint(f"The downloaded file {archiveFilePath} hash was {filehash} as expected.")
        except Exception:
            # A error occurred removing the targetPath
            if IOUtil.IsFile(archiveFilePath):
                self.LogPrint(f"* A error occurred removing '{archiveFilePath}' to be safe.")
                IOUtil.RemoveFile(archiveFilePath)
            raise

    @staticmethod
    def __GetFileNameFromUrl(url: str) -> str:
        index = url.rindex("/")
        if index < 0:
            raise Exception(f"Could not extract the filename from URL '{url}'")
        return url[index + 1 :]

    @staticmethod
    def GetTargetFilename(sourceCommand: XmlRecipePipelineFetchCommandDownload) -> str:
        return PipelineCommandDownload.__GetFileNameFromUrl(sourceCommand.URL) if sourceCommand.To is None else sourceCommand.To

    @staticmethod
    def IsValidCacheFile(cachePath: str, sourceCommand: XmlRecipePipelineFetchCommandDownload) -> bool:
        if not IOUtil.IsFile(cachePath):
            return False
        filehash = PipelineCommandFetch.GenerateFileHash(cachePath)
        return filehash == sourceCommand.Hash


class PipelineCommandGitClone(PipelineCommandFetch):
    def __init__(self, log: Log, sourceCommand: XmlRecipePipelineFetchCommandGitClone, pipelineInfo: PipelineInfo) -> None:
        super().__init__(log, sourceCommand, pipelineInfo)
        if pipelineInfo.Tasks.TaskGitClone is None:
            raise Exception(f"The '{sourceCommand.CommandName}' operation has not been enabled for this platform")
        self.Task = pipelineInfo.Tasks.TaskGitClone
        self.__SourceCommand = sourceCommand
        self.AutoCreateDstDirectory = False

    def DoExecute(self) -> None:
        # resolved before the try so the error handler below can always reference it
        dstPath = self.Info.DstRootPath
        try:
            if not self.Info.AllowDownloads and not IOUtil.IsDirectory(dstPath):
                raise Exception(
                    f"Could not git clone {self.__SourceCommand.URL} to {dstPath} as downloads have been disabled. Enable downloads or clone it manually."
                )
            remoteTag = self.__SourceCommand.Tag
            self.Task.RunGitClone(self.__SourceCommand.URL, remoteTag, dstPath)
            if len(remoteTag) <= 0 and self.__SourceCommand.Hash is not None:
                self.Task.RunGitCheckout(dstPath, self.__SourceCommand.Hash)

            # get the repo hash
            hashStr = self.Task.GetCurrentHash(dstPath)
            if self.__SourceCommand.Hash is None:
                if self.Log.Verbosity >= 1:
                    self.Log.LogPrintWarning(f"No hash value defined for repo {dstPath} which has a hash value of {hashStr}")
            elif hashStr != self.__SourceCommand.Hash:
                raise Exception(f"The repo {dstPath} has a hash of {hashStr} which did not match the expected value of {self.__SourceCommand.Hash}")
            elif self.Log.Verbosity >= 2:
                self.LogPrint(f"The cloned repo {dstPath} hash was {hashStr} as expected.")
        except Exception:
            self.__DoSafeRemoveDirectoryTree(dstPath, 0)
            raise

    def __DoSafeRemoveDirectoryTree(self, dstPath: str, retryCount: int) -> None:
        # A error occurred removing the targetPath
        if not IOUtil.IsDirectory(dstPath) or retryCount >= 2:
            return

        try:
            self.LogPrint(f"* A error occurred removing '{dstPath}' to be safe.")
            IOUtil.SafeRemoveDirectoryTree(dstPath, False)
        except Exception:
            self.LogPrint(f"* Failed to remove '{dstPath}', trying again in 1sec.")
            time.sleep(1)
            self.__DoSafeRemoveDirectoryTree(dstPath, retryCount + 1)
            raise


class PipelineCommandUnpack(PipelineCommand):
    def __init__(self, log: Log, sourceCommand: XmlRecipePipelineCommandUnpack, pipelineInfo: PipelineInfo) -> None:
        super().__init__(log, sourceCommand, pipelineInfo)
        self.AutoCreateDstDirectory = False
        if pipelineInfo.Tasks.TaskUnpackAndRename is None:
            raise Exception(f"The '{sourceCommand.CommandName}' operation has not been enabled for this platform")
        self.Task = pipelineInfo.Tasks.TaskUnpackAndRename
        self.__SourceCommand = sourceCommand

    def DoExecute(self) -> None:
        packedFilePath = IOUtil.Join(self.Info.SrcRootPath, self.__SourceCommand.File)
        self.Task.RunUnpack(packedFilePath, self.Info.DstRootPath)


class PipelineCommandCMakeBuild(PipelineCommand):
    def __init__(self, log: Log, sourceCommand: XmlRecipePipelineCommandCMakeBuild, pipelineInfo: PipelineInfo, allowSkip: bool) -> None:
        super().__init__(log, sourceCommand, pipelineInfo)
        self.Source = sourceCommand.Source
        if pipelineInfo.Tasks.TaskCMakeAndBuild is None:
            raise Exception(f"The '{sourceCommand.CommandName}' operation has not been enabled for this platform")
        self.Task = pipelineInfo.Tasks.TaskCMakeAndBuild
        self.__SourceCommand = sourceCommand
        self.AllowSkip = allowSkip
        self.VariableDict = self.__BuildVariableDict(log)
        self._IsAndroid = (
            pipelineInfo.SourcePackage.ResolvedPlatform.Name == PlatformNameString.ANDROID if pipelineInfo.SourcePackage.ResolvedPlatform is not None else False
        )
        if self._IsAndroid:
            minVersion = CMakeUtil.GetMinimumVersion()
            try:
                version = CMakeUtil.GetVersion()
                self.Log.LogPrint(f"CMake version {version.Major}.{version.Minor}.{version.Build}")
                if version < minVersion:
                    raise Exception(f"CMake version {minVersion.Major}.{minVersion.Minor}.{minVersion.Build} or greater is required")
            except Exception as e:
                self.Log.DoPrintWarning(
                    f"Failed to determine CMake version, please ensure you have {minVersion.Major}.{minVersion.Minor}.{minVersion.Build} or better available."
                )
                self.Log.LogPrintWarning(str(e))

    def DoExecute(self) -> None:
        sourcePackage = self.Info.SourcePackage
        recipeVariants = sourcePackage.ResolvedRecipeVariants
        packageToolFinder = PackageToolFinder(sourcePackage.ResolvedToolDependencyOrder)
        optionList = shlex.split(self.__SourceCommand.Options)
        optionList = self.__ApplyVariables(optionList)
        target = self.__SourceCommand.Target
        installPath = self.FinalDstPath
        sourcePath = self.Info.SrcRootPath
        if self.Source is not None:
            sourcePath = IOUtil.Join(sourcePath, self.Source)
        self.__RunCMakeAndBuild(
            sourcePackage,
            packageToolFinder,
            recipeVariants,
            sourcePath,
            installPath,
            self.Info.DstRootPath,
            target,
            self.__SourceCommand.Project,
            self.__SourceCommand.ConfigurationList,
            optionList,
            self.AllowSkip,
        )

    def __RunCMakeAndBuild(
        self,
        sourcePackage: Package,
        toolFinder: PackageToolFinder,
        recipeVariants: list[str],
        sourcePath: str,
        installPath: str,
        tempBuildPath: str,
        target: CMakeTargetType,
        cmakeProjectName: str,
        configurationList: list[BuildVariantConfig],
        cmakeOptionList: list[str],
        allowSkip: bool,
    ) -> None:
        installedDependencyList = self.__BuildDependencyPathList(sourcePackage, sourcePackage.ResolvedExperimentalRecipeBuildOrder)
        if len(recipeVariants) <= 0:
            installedDependencies = self.__BuildDependencyPathString(installedDependencyList)
            if installedDependencies is not None and len(installedDependencies) > 0:
                cmakeOptionList = list(cmakeOptionList)
                cmakeOptionList.append(installedDependencies)

            self.Task.RunCMakeAndBuild(
                toolFinder, sourcePath, installPath, tempBuildPath, target, cmakeProjectName, configurationList, cmakeOptionList, allowSkip
            )
            return

        for variant in recipeVariants:
            self.Log.LogPrint(f"Recipe variant: {variant}")
            self.Log.PushIndent()
            try:
                cmakeOptionListCopy = list(cmakeOptionList)
                if len(installedDependencyList) > 0:
                    installedDependencyListCopy = [IOUtil.Join(entry, variant) for entry in installedDependencyList]
                    installedDependencies = self.__BuildDependencyPathString(installedDependencyListCopy)
                    cmakeOptionListCopy.append(installedDependencies)

                if self._IsAndroid:
                    # FIX: Set this depending on package type
                    if not AndroidUtil.UseNDKCMakeToolchain():
                        optionList = [f"-DCMAKE_SYSTEM_VERSION={AndroidUtil.GetTargetSDKVersion()}", f"-DCMAKE_ANDROID_ARCH_ABI={variant}"]
                    else:
                        optionList = [f"-DANDROID_PLATFORM=android-{AndroidUtil.GetTargetSDKVersion()}", f"-DANDROID_ABI={variant}"]
                    cmakeOptionListCopy += optionList

                installPathCopy = IOUtil.Join(installPath, variant)
                tempBuildPathCopy = IOUtil.Join(tempBuildPath, variant)
                IOUtil.SafeMakeDirs(tempBuildPathCopy)

                self.Task.RunCMakeAndBuild(
                    toolFinder, sourcePath, installPathCopy, tempBuildPathCopy, target, cmakeProjectName, configurationList, cmakeOptionListCopy, allowSkip
                )
            finally:
                self.Log.PopIndent()

    def __BuildDependencyPathString(self, dependencyPathList: list[str]) -> str:
        packageInstallDirs = dependencyPathList
        if len(packageInstallDirs) <= 0:
            return ""
        strDirs = ";".join(packageInstallDirs)
        return f"-DCMAKE_PREFIX_PATH={strDirs}"

    def __BuildDependencyPathList(self, sourcePackage: Package, resolvedBuildOrder: list[Package]) -> list[str]:
        dependencyDirList = []
        for package in resolvedBuildOrder:
            if package != sourcePackage and package.Type != PackageType.ToolRecipe:
                if package.ResolvedDirectExperimentalRecipe is not None and package.ResolvedDirectExperimentalRecipe.ResolvedInstallLocation is not None:
                    dependencyDirList.append(package.ResolvedDirectExperimentalRecipe.ResolvedInstallLocation.ResolvedPath)
        return dependencyDirList

    def __ApplyVariables(self, optionList: list[str]) -> list[str]:
        result: list[str] = []
        for option in optionList:
            parsedString = ParsedFormatString(option, self.VariableDict)
            if len(parsedString.VarCommandList) <= 0:
                result.append(option)
            else:
                for var in parsedString.VarCommandList:
                    parsedString.SplitList[var.SplitIndex] = var.Report.Options[0]
                result.append("".join(parsedString.SplitList))
        return result

    def __BuildVariableDict(self, log: Log) -> StringVariableDict:
        variables = StringVariableDict()
        if self.Info.RecipeAbsolutePath is not None:
            variables.Add("RECIPE_PATH", self.Info.RecipeAbsolutePath)
        variables.Add("DST_PATH", self.Info.DstRootPath)
        variables.Add("SRC_PATH", self.Info.SrcRootPath)
        variables.Add("OUTPUT_PATH", self.FinalDstPath)
        return variables


class PipelineCommandCombine(PipelineCommand):
    def __init__(self, log: Log, sourceCommand: XmlRecipePipelineCommandCombine, pipelineInfo: PipelineInfo, sourceRecipeName: str) -> None:
        super().__init__(log, sourceCommand, pipelineInfo)
        self.__SourceCommand = sourceCommand
        self.CommandList = self.__GenerateCommandList(log, sourceCommand.CommandList, pipelineInfo, sourceRecipeName)

    def DoExecute(self) -> None:
        for command in self.CommandList:
            command.Execute()

    def __GenerateCommandList(
        self, log: Log, commandList: list[XmlRecipePipelineBuildCommand], pipelineInfo: PipelineInfo, sourceRecipeName: str
    ) -> list[PipelineCommand]:
        theList: list[PipelineCommand] = []
        commandIndex = 0
        for command in commandList:
            dstSubDir = f"{commandIndex:04}"
            commandDstRootPath = IOUtil.Join(pipelineInfo.DstRootPath, dstSubDir)
            theList.append(self.__CreateCommand(log, command, pipelineInfo, commandDstRootPath, pipelineInfo.DstRootPathReadOnly, sourceRecipeName))
            commandIndex = commandIndex + 1
        return theList

    def __CreateCommand(
        self, log: Log, sourceCommand: XmlRecipePipelineCommand, pipelineInfo: PipelineInfo, dstRootPath: str, dstRootPathReadOnly: bool, sourceRecipeName: str
    ) -> PipelineCommand:
        if sourceCommand.CommandType == BuildRecipePipelineCommand.CMakeBuild:
            if not isinstance(sourceCommand, XmlRecipePipelineCommandCMakeBuild):
                raise Exception("Internal error, sourceCommand was not XmlRecipePipelineCommandCMakeBuild")
            info = PipelineInfo(
                pipelineInfo.Tasks,
                pipelineInfo.SourcePackage,
                pipelineInfo.PathBuilder,
                pipelineInfo.SrcRootPath,
                pipelineInfo.SrcRootPathReadOnly,
                dstRootPath,
                dstRootPathReadOnly,
                combinedDstRootPath=pipelineInfo.DstRootPath,
            )
            return PipelineCommandCMakeBuild(log, sourceCommand, info, False)
        raise Exception(f"Unsupported combined command '{sourceCommand.CommandType}' in '{sourceRecipeName}'")


class PipelineCommandCopy(PipelineCommand):
    def __init__(self, log: Log, sourceCommand: XmlRecipePipelineCommandCopy, pipelineInfo: PipelineInfo, sourceRecipeName: str) -> None:
        super().__init__(log, sourceCommand, pipelineInfo)
        self.AutoCreateDstDirectory = False

    def IsCompleted(self) -> bool:
        return IOUtil.Exists(self.Info.DstRootPath)

    def DoExecute(self) -> None:
        if self.IsCompleted():
            self.LogPrint(f"Copy from '{self.Info.SrcRootPath}' to '{self.Info.DstRootPath}' skipped as target exist")
            return
        self.LogPrint(f"Copy from '{self.Info.SrcRootPath}' to '{self.Info.DstRootPath}'")

        # TODO: we could allow the copy command to have a 'patterns' string that can be forwarded to shutil.ignore_patterns
        shutil.copytree(self.Info.SrcRootPath, self.Info.DstRootPath)
        # example of a ignore pattern
        # shutil.copytree(self.Info.SrcRootPath, self.Info.DstRootPath, ignore=shutil.ignore_patterns('.git'))


class PipelineCommandInstall(PipelineCommand):
    def __init__(self, log: Log, pipelineInfo: PipelineInfo) -> None:
        super().__init__(log, None, pipelineInfo)
        self.AutoCreateDstDirectory = False

    def IsCompleted(self) -> bool:
        return IOUtil.Exists(self.Info.DstRootPath)

    def DoExecute(self) -> None:
        if self.IsCompleted():
            self.LogPrint(f"Installing from '{self.Info.SrcRootPath}' to '{self.Info.DstRootPath}' skipped as target exist")
            return
        self.LogPrint(f"Installing from '{self.Info.SrcRootPath}' to '{self.Info.DstRootPath}'")

        if not self.Info.SrcRootPathReadOnly:
            shutil.move(self.Info.SrcRootPath, self.Info.DstRootPath)
        else:
            shutil.copytree(self.Info.SrcRootPath, self.Info.DstRootPath)


class PipelineCommandBuilder:
    def __init__(self, generatorContext: GeneratorContext, checkBuildCommands: bool, buildThreads: int) -> None:
        super().__init__()
        self.__Log = generatorContext.Log
        self.__AllowDownloads = generatorContext.AllowDownloads

        # Add the task objects
        self.PipelineTasks = PipelineTasks(self.__Log, generatorContext, checkBuildCommands, buildThreads)

        # TODO: enable this once we fixed the path issue (beware code moved to path builder now)
        # if not self.TaskCMakeAndBuild is None:
        #    shortId = self.TaskCMakeAndBuild.CompilerShortId
        #    installRootPath = IOUtil.Join(installRootPath, shortId)

        self.__PathBuilder = generatorContext.RecipePathBuilder
        self.__CommandList: list[PipelineCommand] | None = None
        self.__SourcePackage: Package | None = None
        self.__SourceRecipe: PackageExperimentalRecipe | None = None
        self.__CommandInputRootPath: str | None = None
        self.__CommandInputRootPathReadOnly = True
        self.__PipelineInstallPath: str | None = None

    def GetBuildPath(self, sourceRecipe: PackageExperimentalRecipe) -> str:
        return self.__PathBuilder.GetBuildPath(sourceRecipe)

    def Begin(self, sourcePackage: Package, sourceRecipe: PackageExperimentalRecipe) -> None:
        """Starts a new list, even if the previous begin operation wasn't ended"""
        self.__CommandList = []
        self.__SourcePackage = sourcePackage
        self.__SourceRecipe = sourceRecipe
        self.__CommandInputRootPath = sourcePackage.AbsolutePath
        self.__CommandInputRootPathReadOnly = True
        self.__PipelineInstallPath = sourceRecipe.ResolvedInstallLocation.ResolvedPath if sourceRecipe.ResolvedInstallLocation is not None else None

    def Add(self, sourceCommand: XmlRecipePipelineCommand, skip: bool) -> None:
        if self.__CommandList is None or self.__CommandInputRootPath is None:
            raise Exception("Usage error, Add called outside begin/end block")

        command = self.__CreateCommand(sourceCommand, self.__CommandInputRootPath, self.__CommandInputRootPathReadOnly)
        command.Skip = skip
        self.__CommandList.append(command)
        if not skip:
            # The next command in the pipeline uses the 'output' from the previous command as input
            self.__CommandInputRootPath = command.FinalDstPath
            self.__CommandInputRootPathReadOnly = command.Info.DstRootPathReadOnly

    def End(self) -> list[PipelineCommand]:
        if (
            self.__SourcePackage is None
            or self.__PathBuilder is None
            or self.__CommandInputRootPath is None
            or self.__PipelineInstallPath is None
            or self.__CommandList is None
            or self.__SourceRecipe is None
        ):
            raise Exception("Usage error, End called outside begin/end block")

        # Add a final install command to finish the pipe
        installInfo = PipelineInfo(
            self.PipelineTasks,
            self.__SourcePackage,
            self.__PathBuilder,
            self.__CommandInputRootPath,
            self.__CommandInputRootPathReadOnly,
            self.__PipelineInstallPath,
            False,
        )
        pipelineInstallCommand = PipelineCommandInstall(self.__Log, installInfo)
        self.__CommandList.append(pipelineInstallCommand)

        result = self.__CommandList

        if pipelineInstallCommand.IsCompleted():
            self.__Log.LogPrint(f"  Pipeline '{self.__SourceRecipe.FullName}' skipped as target path '{installInfo.DstRootPath}' exists.")
            result = []

        self.__CommandList = None
        self.__SourcePackage = None
        self.__SourceRecipe = None
        self.__CommandInputRootPath = None
        self.__CommandInputRootPathReadOnly = True
        self.__PipelineInstallPath = None
        return result

    def __GetTempDirectoryName(self, sourceCommand: XmlRecipePipelineCommand) -> str:
        if self.__SourcePackage is None or self.__CommandList is None:
            raise Exception("__GetTempDirectoryName called outside begin/end block")
        if self.__SourcePackage.ResolvedDirectExperimentalRecipe is None:
            raise Exception("Invalid package")

        rootPath = self.GetBuildPath(self.__SourcePackage.ResolvedDirectExperimentalRecipe)
        stepString = f"{len(self.__CommandList):0>4}_{sourceCommand.CommandName}"
        return IOUtil.Join(rootPath, stepString)

    def __CreateCommand(self, sourceCommand: XmlRecipePipelineCommand, srcRootPath: str, srcRootPathReadOnly: bool) -> PipelineCommand:
        if self.__SourcePackage is None or self.__SourceRecipe is None:
            raise Exception("Invalid state")

        if sourceCommand.CommandType == BuildRecipePipelineCommand.Download:
            if not isinstance(sourceCommand, XmlRecipePipelineFetchCommandDownload):
                raise Exception("Internal error, sourceCommand was not XmlRecipePipelineFetchCommandDownload")
            return self.__CreateCommandDownload(sourceCommand, srcRootPath)
        elif sourceCommand.CommandType == BuildRecipePipelineCommand.GitClone:
            if not isinstance(sourceCommand, XmlRecipePipelineFetchCommandGitClone):
                raise Exception("Internal error, sourceCommand was not XmlRecipePipelineFetchCommandGitClone")
            return self.__CreateCommandGitClone(sourceCommand, srcRootPath)
        elif sourceCommand.CommandType == BuildRecipePipelineCommand.Source:
            if not isinstance(sourceCommand, XmlRecipePipelineFetchCommandSource):
                raise Exception("Internal error, sourceCommand was not XmlRecipePipelineFetchCommandSource")
            return self.__CreateCommandSource(sourceCommand, srcRootPath)
        elif sourceCommand.CommandType == BuildRecipePipelineCommand.Unpack:
            if not isinstance(sourceCommand, XmlRecipePipelineCommandUnpack):
                raise Exception("Internal error, sourceCommand was not XmlRecipePipelineCommandUnpack")
            info = PipelineInfo(
                self.PipelineTasks,
                self.__SourcePackage,
                self.__PathBuilder,
                srcRootPath,
                srcRootPathReadOnly,
                self.__GetTempDirectoryName(sourceCommand),
                False,
            )
            return PipelineCommandUnpack(self.__Log, sourceCommand, info)
        elif sourceCommand.CommandType == BuildRecipePipelineCommand.CMakeBuild:
            if not isinstance(sourceCommand, XmlRecipePipelineCommandCMakeBuild):
                raise Exception("Internal error, sourceCommand was not XmlRecipePipelineCommandCMakeBuild")
            info = PipelineInfo(
                self.PipelineTasks,
                self.__SourcePackage,
                self.__PathBuilder,
                srcRootPath,
                srcRootPathReadOnly,
                self.__GetTempDirectoryName(sourceCommand),
                False,
            )
            return PipelineCommandCMakeBuild(self.__Log, sourceCommand, info, True)
        elif sourceCommand.CommandType == BuildRecipePipelineCommand.Combine:
            if not isinstance(sourceCommand, XmlRecipePipelineCommandCombine):
                raise Exception("Internal error, sourceCommand was not XmlRecipePipelineCommandCombine")
            info = PipelineInfo(
                self.PipelineTasks,
                self.__SourcePackage,
                self.__PathBuilder,
                srcRootPath,
                srcRootPathReadOnly,
                self.__GetTempDirectoryName(sourceCommand),
                False,
            )
            return PipelineCommandCombine(self.__Log, sourceCommand, info, self.__SourceRecipe.FullName)
        elif sourceCommand.CommandType == BuildRecipePipelineCommand.Copy:
            if not isinstance(sourceCommand, XmlRecipePipelineCommandCopy):
                raise Exception("Internal error, sourceCommand was not XmlRecipePipelineCommandCopy")
            info = PipelineInfo(
                self.PipelineTasks,
                self.__SourcePackage,
                self.__PathBuilder,
                srcRootPath,
                srcRootPathReadOnly,
                self.__GetTempDirectoryName(sourceCommand),
                False,
            )
            return PipelineCommandCopy(self.__Log, sourceCommand, info, self.__SourceRecipe.FullName)
        raise Exception(f"Unsupported command '{sourceCommand.CommandName}' ({sourceCommand.CommandType}) in '{self.__SourceRecipe.FullName}'")

    def __CreateCommandDownload(self, sourceCommand: XmlRecipePipelineFetchCommandDownload, srcRootPath: str) -> PipelineCommand:
        if self.__SourcePackage is None or self.__SourceRecipe is None:
            raise Exception("Invalid state")

        readonlyCacheRootDir = self.__PathBuilder.ReadonlyCache_DownloadCacheRootPath
        if readonlyCacheRootDir is not None:
            # If we have a download cache and the directory exists there then setup a void fetch command
            targetFilename = PipelineCommandDownload.GetTargetFilename(sourceCommand)
            cachePath = IOUtil.Join(readonlyCacheRootDir, targetFilename)
            self.__Log.LogPrintVerbose(3, f"Checking readonly cache file '{cachePath}'")
            if PipelineCommandDownload.IsValidCacheFile(cachePath, sourceCommand):
                self.__Log.LogPrintVerbose(3, f"Using readonly cache file '{cachePath}'")
                info = PipelineInfo(self.PipelineTasks, self.__SourcePackage, self.__PathBuilder, readonlyCacheRootDir, True, readonlyCacheRootDir, True)
                return PipelineCommandNOP(self.__Log, sourceCommand, info)
            else:
                self.__Log.LogPrintVerbose(3, f"Readonly cache file was not valid '{cachePath}'")

        if self.__PathBuilder.DownloadCacheRootPath is None:
            raise Exception("Invalid State")

        info = PipelineInfo(
            self.PipelineTasks,
            self.__SourcePackage,
            self.__PathBuilder,
            srcRootPath,
            True,
            self.__PathBuilder.DownloadCacheRootPath,
            True,
            allowDownloads=self.__AllowDownloads,
        )
        return PipelineCommandDownload(self.__Log, sourceCommand, info)

    def __CreateCommandGitClone(self, sourceCommand: XmlRecipePipelineFetchCommandGitClone, srcRootPath: str) -> PipelineCommand:
        if self.__SourcePackage is None or self.__SourceRecipe is None:
            raise Exception("Invalid state")

        readonlyCacheRootDir = self.__PathBuilder.ReadonlyCache_DownloadCacheRootPath
        if readonlyCacheRootDir is not None:
            cachePath = IOUtil.Join(readonlyCacheRootDir, self.__SourceRecipe.FullName)
            self.__Log.LogPrintVerbose(3, f"Checking readonly cache directory '{cachePath}'")
            if IOUtil.IsDirectory(cachePath):
                self.__Log.LogPrintVerbose(3, f"Using readonly cache directory '{cachePath}'")
                info = PipelineInfo(self.PipelineTasks, self.__SourcePackage, self.__PathBuilder, cachePath, True, cachePath, True)
                return PipelineCommandNOP(self.__Log, sourceCommand, info)
            else:
                self.__Log.LogPrintVerbose(3, f"Readonly cache directory '{cachePath}' not found")

        if self.__PathBuilder.DownloadCacheRootPath is None:
            raise Exception("Invalid State")

        dstPath = IOUtil.Join(self.__PathBuilder.DownloadCacheRootPath, self.__SourceRecipe.FullName)
        info = PipelineInfo(
            self.PipelineTasks, self.__SourcePackage, self.__PathBuilder, srcRootPath, True, dstPath, True, allowDownloads=self.__AllowDownloads
        )
        return PipelineCommandGitClone(self.__Log, sourceCommand, info)

    def __CreateCommandSource(self, sourceCommand: XmlRecipePipelineFetchCommandSource, srcRootPath: str) -> PipelineCommand:
        if self.__SourcePackage is None or self.__SourceRecipe is None:
            raise Exception("Invalid state")

        # We basically setup a NOP command that points to the source package location which will allow the pipeline to work with that
        info = PipelineInfo(
            self.PipelineTasks, self.__SourcePackage, self.__PathBuilder, srcRootPath, True, srcRootPath, True, allowDownloads=self.__AllowDownloads
        )
        return PipelineCommandNOP(self.__Log, sourceCommand, info)
