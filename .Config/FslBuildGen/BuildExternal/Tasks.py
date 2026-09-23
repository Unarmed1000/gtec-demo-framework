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

import os
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
from collections.abc import Callable

from FslBuildGen import IOUtil, __version__

# from FslBuildGen import PackageConfig
from FslBuildGen.Build.BuildUtil import PlatformBuildTypeInfo, PlatformBuildUtil
from FslBuildGen.BuildExternal import CMakeHelper
from FslBuildGen.BuildExternal.CMakeTypes import CMakeBuildType, CMakeGeneratorMultiConfigCapability, CMakeGeneratorName
from FslBuildGen.BuildExternal.FileUnpacker import FileUnpack
from FslBuildGen.CMakeUtil import CMakeVersion
from FslBuildGen.Context.GeneratorContext import GeneratorContext

# from FslBuildGen.DataTypes import BuildThreads
from FslBuildGen.DataTypes import BuildPlatformType, BuildVariantConfig, CMakeTargetType

# from FslBuildGen.PackageConfig import PlatformNameString
from FslBuildGen.GitUtil import GitUtil
from FslBuildGen.PackageToolFinder import PackageToolFinder
from FslBuildGen.PlatformUtil import PlatformUtil


class BasicTask:
    def __init__(self, generatorContext: GeneratorContext) -> None:
        super().__init__()
        self.Context = generatorContext
        self.Log = generatorContext.Log
        # self._IsAndroid = generatorContext.PlatformName == PlatformNameString.ANDROID

    def LogPrint(self, message: str) -> None:
        self.Log.LogPrint(message)

    def LogPrintWarning(self, message: str) -> None:
        self.Log.LogPrintWarning(message)

    def DoPrint(self, message: str) -> None:
        self.Log.DoPrint(message)

    def CreateDirectory(self, path: str) -> None:
        if not IOUtil.IsDirectory(path):
            self.Log.LogPrint(f"Creating '{path}' as it was missing")
            IOUtil.SafeMakeDirs(path)


class DownloadTask(BasicTask):
    # Some servers (like registry.khronos.org) reject the default 'Python-urllib' user agent
    USER_AGENT = f"FslBuild/{__version__}"
    BLOCK_SIZE = 64 * 1024

    # def __init__(self, generatorContext: GeneratorContext) -> None:
    #    super().__init__(generatorContext)

    @staticmethod
    def __DownloadReport(fnLogPrint: Callable[[str], None], startTime: float, shortFileName: str, count: int, blockSize: int, totalSize: int) -> None:
        if totalSize <= 0:
            return

        duration = time.time() - startTime
        progressSize = int(count * blockSize)
        speed = int(progressSize / (1024 * duration)) if duration > 0 else 0
        percent = min(int(count * blockSize * 100 / totalSize), 100)
        fnLogPrint(f"* {shortFileName}: {percent:3d}% of {totalSize / (1024 * 1024)} MB, {speed} KB/s, {duration:.2f} seconds passed.")

    @staticmethod
    def __MakeDownReporter(fnLogPrint: Callable[[str], None], shortFileName: str) -> Callable[[int, int, int], None]:
        startTime = time.time()
        return lambda x, y, z: DownloadTask.__DownloadReport(fnLogPrint, startTime, shortFileName, x, y, z)

    def DownloadFromUrl(self, url: str, dstPath: str) -> None:
        if IOUtil.IsFile(dstPath):
            self.LogPrint(f"Downloaded archive found at '{dstPath}', skipping download.")
            return

        self.DoPrint(f"Downloading '{url}' to '{dstPath}'")
        reporter = DownloadTask.__MakeDownReporter(self.DoPrint, IOUtil.GetFileName(dstPath))
        # Download to a temporary file first so a interrupted download is never mistaken for a completed one
        tmpPath = f"{dstPath}.part"
        request = urllib.request.Request(url, headers={"User-Agent": DownloadTask.USER_AGENT})
        try:
            with urllib.request.urlopen(request) as response, open(tmpPath, "wb") as dstFile:
                contentLength = response.headers.get("Content-Length")
                totalSize = int(contentLength) if contentLength is not None else -1
                count = 0
                reporter(count, DownloadTask.BLOCK_SIZE, totalSize)
                while True:
                    block = response.read(DownloadTask.BLOCK_SIZE)
                    if not block:
                        break
                    dstFile.write(block)
                    count += 1
                    reporter(count, DownloadTask.BLOCK_SIZE, totalSize)
            os.replace(tmpPath, dstPath)
        except BaseException:
            if os.path.isfile(tmpPath):
                os.remove(tmpPath)
            raise


class GitBaseTask(BasicTask):
    def __init__(self, generatorContext: GeneratorContext) -> None:
        super().__init__(generatorContext)
        self.__ConfigureForPlatform(generatorContext)

    def __ConfigureForPlatform(self, generatorContext: GeneratorContext) -> None:
        self.GitCommand = GitUtil.GetExecutableName(generatorContext.Generator.PlatformName)


class GitCloneTask(GitBaseTask):
    # def __init__(self, generatorContext: GeneratorContext) -> None:
    #    super().__init__(generatorContext)

    def RunGitClone(self, sourcePath: str, branch: str, targetPath: str) -> None:
        if IOUtil.IsDirectory(targetPath):
            self.LogPrint(f"Running git clone {sourcePath} {targetPath}, skipped since it exist.")
            return

        self.DoPrint(f"Running git clone {sourcePath} {targetPath}")
        try:
            self.__RunGitClone(sourcePath, targetPath, branch)
        except Exception:
            # A error occurred removing the targetPath
            self.LogPrint(f"* A error occurred removing '{targetPath}' to be safe.")
            IOUtil.SafeRemoveDirectoryTree(targetPath, True)
            raise

    def RunGitCheckout(self, sourcePath: str, branch: str) -> None:
        self.DoPrint(f"Running git checkout {branch} at {sourcePath}")
        try:
            self.__RunGitCheckout(sourcePath, branch)
        except Exception:
            # A error occurred removing the targetPath
            raise

    def GetCurrentHash(self, path: str) -> str:
        return GitUtil.GetCurrentHash(self.GitCommand, path)

    def __RunGitClone(self, sourcePath: str, targetPath: str, branch: str) -> None:
        buildCommand = [self.GitCommand, "clone", sourcePath, targetPath]
        # for faster checkout
        ##--single-branch --depth 1
        buildCommand += ["--single-branch"]
        if branch is not None and len(branch) > 0:
            buildCommand += ["-b", branch]
        result = subprocess.call(buildCommand)
        if result != 0:
            raise Exception(f"git clone failed {buildCommand}")

    def __RunGitCheckout(self, sourcePath: str, branch: str) -> None:
        if len(branch) <= 0:
            raise Exception("A git checkout branch name can not be empty")

        currentWorkingDirectory = sourcePath
        buildCommand = [self.GitCommand, "checkout", branch]
        result = subprocess.call(buildCommand, cwd=currentWorkingDirectory)
        if result != 0:
            self.LogPrintWarning("The command '{}' failed with '{}'. It was run with CWD: '{}'".format(" ".join(buildCommand), result, currentWorkingDirectory))
            raise Exception(f"git clone failed {buildCommand}")


class GitApplyTask(GitBaseTask):
    # def __init__(self, generatorContext: GeneratorContext) -> None:
    #    super().__init__(generatorContext)

    def RunGitApply(self, sourcePatchFile: str, targetPath: str) -> None:
        self.LogPrint(f"Running git apply {sourcePatchFile} in {targetPath}")
        buildCommand = [self.GitCommand, "apply", sourcePatchFile, "--whitespace=fix", "--ignore-space-change", "--ignore-whitespace"]
        if self.Log.Verbosity > 0:
            buildCommand.append("-v")
        result = subprocess.call(buildCommand, cwd=targetPath)
        if result != 0:
            raise Exception(f"git apply failed {buildCommand}")


class UnpackAndRenameTask(BasicTask):
    # def __init__(self, generatorContext: GeneratorContext) -> None:
    #    super().__init__(generatorContext)

    def RunUnpack(self, packedFilePath: str, dstPath: str) -> None:
        if not IOUtil.IsDirectory(dstPath):
            self.__RunUnpack(packedFilePath, dstPath)
        else:
            self.LogPrint(f"Unpacked directory found at '{dstPath}', skipping unpack.")

    def __RunUnpack(self, packedFilePath: str, dstPath: str) -> None:
        self.LogPrint(f"* Unpacking archive '{packedFilePath}' to '{dstPath}'")
        FileUnpack.UnpackFile(packedFilePath, dstPath)


class CMakeBuilder:
    def __init__(self, generatorContext: GeneratorContext, buildThreads: int, buildTypeInfo: PlatformBuildTypeInfo) -> None:
        super().__init__()
        self.Context = generatorContext
        self.Log = generatorContext.Log
        # Builders like ninja and make only contains a single configuration
        self.IsSingleConfiguration = False
        #        #self.__ConfigureForPlatform(generatorContext)
        self.BuilderThreadArguments: list[str] = []
        self.NumBuildThreads = PlatformBuildUtil.AddBuildThreads(
            generatorContext.Log, self.BuilderThreadArguments, generatorContext.PlatformName, buildThreads, buildTypeInfo, generatorContext.CMakeConfig, True
        )

    def Execute(
        self,
        toolFinder: PackageToolFinder,
        path: str,
        target: CMakeTargetType,
        cmakeProjectName: str,
        configuration: BuildVariantConfig,
        buildEnv: dict[str, str],
        parentPath: str,
    ) -> None:
        pass


class CMakeBuilderDummy(CMakeBuilder):
    def __init__(self, generatorContext: GeneratorContext, buildThreads: int) -> None:
        super().__init__(generatorContext, buildThreads, PlatformBuildTypeInfo.CMakeCustom)
        self.IsSingleConfiguration = False

    def Execute(
        self,
        toolFinder: PackageToolFinder,
        path: str,
        target: CMakeTargetType,
        cmakeProjectName: str,
        configuration: BuildVariantConfig,
        buildEnv: dict[str, str],
        parentPath: str,
    ) -> None:
        raise Exception("This builder's Execute method is not supposed to be called")


class CMakeBuilderGeneric(CMakeBuilder):
    MINIMUM_VERSION = CMakeVersion(3, 16, 0)

    def __init__(self, generatorContext: GeneratorContext, buildThreads: int) -> None:
        super().__init__(generatorContext, buildThreads, PlatformBuildTypeInfo.CMake)

        cmakeConfig = generatorContext.CMakeConfig
        if cmakeConfig.CMakeVersion < CMakeBuilderGeneric.MINIMUM_VERSION:
            raise Exception(
                f"The chosen CMake generator '{cmakeConfig.CMakeFinalGeneratorName}' requires cmake version {CMakeBuilderGeneric.MINIMUM_VERSION} or newer"
            )

        # The cmake make files only support one configuration
        self.IsSingleConfiguration = (
            CMakeHelper.GetGeneratorMultiConfigCapabilities(cmakeConfig.CMakeFinalGeneratorName) != CMakeGeneratorMultiConfigCapability.Yes
        )
        self.CMakeConfig = cmakeConfig

    def Execute(
        self,
        toolFinder: PackageToolFinder,
        path: str,
        target: CMakeTargetType,
        cmakeProjectName: str,
        configuration: BuildVariantConfig,
        buildEnv: dict[str, str],
        parentPath: str,
    ) -> None:
        self.Log.LogPrint(f"* Running make at '{path}' for project '{cmakeProjectName}' and configuration '{BuildVariantConfig.ToString(configuration)}'")

        cmakeCommand = self.CMakeConfig.CMakeCommand
        cmakeConfig = CMakeBuildType.FromBuildVariantConfig(configuration)
        buildCommand = [cmakeCommand, "--build", path, "--config", cmakeConfig]
        if self.CMakeConfig.EmscriptenEnabled:
            buildCommand.insert(0, self.CMakeConfig.EmscriptenBuildCommand)

        if self.NumBuildThreads > 0:
            buildCommand += ["--parallel", str(self.NumBuildThreads)]

        try:
            result = subprocess.call(buildCommand, cwd=parentPath, env=buildEnv)
            if result != 0:
                raise Exception(f"cmake failed {buildCommand}")
        except Exception:
            self.Log.LogPrint(f"* cmake failed '{buildCommand}'")
            raise

        if target == CMakeTargetType.Install:
            buildCommand = [cmakeCommand, "--install", path, "--config", cmakeConfig]
            if self.CMakeConfig.EmscriptenEnabled:
                buildCommand.insert(0, self.CMakeConfig.EmscriptenBuildCommand)
            try:
                result = subprocess.call(buildCommand, cwd=parentPath, env=buildEnv)
                if result != 0:
                    raise Exception(f"cmake failed {buildCommand}")
            except Exception:
                self.Log.LogPrint(f"* cmake failed '{buildCommand}'")
                raise


class CMakeBuilderNinja(CMakeBuilder):
    def __init__(self, generatorContext: GeneratorContext, buildThreads: int, useRecipe: bool = True) -> None:
        super().__init__(generatorContext, buildThreads, PlatformBuildTypeInfo.CMakeCustom)
        self.IsSingleConfiguration = True
        self.__CommandName = PlatformUtil.GetPlatformDependentExecuteableName("ninja", PlatformUtil.DetectBuildPlatformType())
        self.__UseRecipe = useRecipe

    def Execute(
        self,
        toolFinder: PackageToolFinder,
        path: str,
        target: CMakeTargetType,
        cmakeProjectName: str,
        configuration: BuildVariantConfig,
        buildEnv: dict[str, str],
        parentPath: str,
    ) -> None:
        projectFile = "rules.ninja"

        if self.__UseRecipe:
            toolPackage = toolFinder.GetToolPackageByToolName("ninja")
            commandName = IOUtil.Join(toolPackage.AbsoluteToolPath, self.__CommandName)
        else:
            commandName = self.__CommandName

        self.Log.LogPrint(f"* Running ninja at '{path}' for project '{projectFile}' and configuration '{BuildVariantConfig.ToString(configuration)}'")
        buildCommand = [commandName]
        if target == CMakeTargetType.Install:
            buildCommand.append("install")

        buildCommand += self.BuilderThreadArguments

        try:
            result = subprocess.call(buildCommand, cwd=path, env=buildEnv)
            if result != 0:
                raise Exception(f"ninja failed with {result} command {buildCommand}")
        except Exception:
            self.Log.LogPrint(f"* ninja failed '{buildCommand}'")
            raise


class CMakeBuilderMake(CMakeBuilder):
    def __init__(self, generatorContext: GeneratorContext, buildThreads: int) -> None:
        super().__init__(generatorContext, buildThreads, PlatformBuildTypeInfo.CMakeCustom)
        # The cmake make files only support one configuration
        self.IsSingleConfiguration = True

    def Execute(
        self,
        toolFinder: PackageToolFinder,
        path: str,
        target: CMakeTargetType,
        cmakeProjectName: str,
        configuration: BuildVariantConfig,
        buildEnv: dict[str, str],
        parentPath: str,
    ) -> None:
        projectFile = "Makefile"

        self.Log.LogPrint(f"* Running make at '{path}' for project '{projectFile}' and configuration '{BuildVariantConfig.ToString(configuration)}'")
        buildCommand = ["make", "-f", projectFile]
        buildCommand += self.BuilderThreadArguments
        if target == CMakeTargetType.Install:
            buildCommand.append("install")
        try:
            result = subprocess.call(buildCommand, cwd=path, env=buildEnv)
            if result != 0:
                raise Exception(f"make failed {buildCommand}")
        except Exception:
            self.Log.LogPrint(f"* make failed '{buildCommand}'")
            raise


class CMakeBuilderMSBuild(CMakeBuilder):
    def __init__(self, generatorContext: GeneratorContext, buildThreads: int) -> None:
        super().__init__(generatorContext, buildThreads, PlatformBuildTypeInfo.CMakeCustom)

    # msbuild INSTALL.vcxproj /p:Configuration=Debug
    # msbuild INSTALL.vcxproj /p:Configuration=Release
    def Execute(
        self,
        toolFinder: PackageToolFinder,
        path: str,
        target: CMakeTargetType,
        cmakeProjectName: str,
        configuration: BuildVariantConfig,
        buildEnv: dict[str, str],
        parentPath: str,
    ) -> None:
        projectFile = self.__GetMSBuildFilename(target, cmakeProjectName)
        configurationString = self.__GetMSBuildBuildVariantConfigString(configuration)
        self.Log.LogPrint(f"* Running msbuild at '{path}' for project '{projectFile}' and configuration '{configurationString}'")
        configurationString = f"/p:Configuration={configurationString}"
        buildCommand = ["msbuild.exe", projectFile, configurationString]
        buildCommand += self.BuilderThreadArguments
        try:
            result = subprocess.call(buildCommand, cwd=path, env=buildEnv)
            if result != 0:
                raise Exception(f"msbuild failed {buildCommand}")
        except Exception:
            self.Log.LogPrint(f"* msbuild failed '{buildCommand}'")
            raise

    def __GetMSBuildFilename(self, target: CMakeTargetType, cmakeProjectName: str) -> str:
        if target == CMakeTargetType.Install:
            return "Install.vcxproj"
        return f"{cmakeProjectName}.sln"

    def __GetMSBuildBuildVariantConfigString(self, configuration: BuildVariantConfig) -> str:
        if configuration == BuildVariantConfig.Debug or configuration == BuildVariantConfig.Coverage:
            return "Debug"
        elif configuration == BuildVariantConfig.Release:
            return "Release"
        else:
            raise Exception(f"Unsupported BuildVariantConfig: {configuration}")


class CMakeAndBuildTask(BasicTask):
    def __init__(self, generatorContext: GeneratorContext, buildThreads: int) -> None:
        super().__init__(generatorContext)
        self.CMakeConfig = generatorContext.CMakeConfig
        self.Builder = self.__DetermineBuilder(self.CMakeConfig.GeneratorName, generatorContext, buildThreads)

    # cmake -G "Visual Studio 14 2015 Win64"
    # -DCMAKE_INSTALL_PREFIX="e:\Work\Down\Windows\final\zlib-1.2.11"
    def RunCMakeAndBuild(
        self,
        toolFinder: PackageToolFinder,
        sourcePath: str,
        installPath: str,
        tempBuildPath: str,
        target: CMakeTargetType,
        cmakeProjectName: str,
        configurationList: list[BuildVariantConfig],
        cmakeOptionList: list[str],
        allowSkip: bool,
    ) -> None:
        if allowSkip and IOUtil.IsDirectory(installPath):
            self.LogPrint(f"Running cmake and build on source '{sourcePath}' and installing to '{installPath}' was skipped since install directory exist.")
            return

        self.LogPrint(f"Running cmake and build on source '{sourcePath}' and installing to '{installPath}'")
        try:
            self.CreateDirectory(tempBuildPath)

            # Add platform specific commands
            if len(self.CMakeConfig.CMakeConfigRecipeArguments) > 0:
                cmakeOptionList += self.CMakeConfig.CMakeConfigRecipeArguments

            buildEnv: dict[str, str] = os.environ.copy()
            self.__ApplyPath(buildEnv, toolFinder.ToolPaths)

            self.__DoBuildNow(toolFinder, sourcePath, installPath, tempBuildPath, target, cmakeProjectName, configurationList, cmakeOptionList, buildEnv)
        except Exception:
            # A error occurred remove the install dir
            self.LogPrint(f"* A error occurred removing '{installPath}' to be safe.")
            IOUtil.SafeRemoveDirectoryTree(installPath, True)
            raise

    def __DoBuildNow(
        self,
        toolFinder: PackageToolFinder,
        sourcePath: str,
        installPath: str,
        tempBuildPath: str,
        target: CMakeTargetType,
        cmakeProjectName: str,
        configurationList: list[BuildVariantConfig],
        cmakeOptionList: list[str],
        buildEnv: dict[str, str],
    ) -> None:
        if not self.Builder.IsSingleConfiguration:
            self.RunCMake(tempBuildPath, sourcePath, installPath, cmakeOptionList, buildEnv)

            for config in configurationList:
                self.Builder.Execute(toolFinder, tempBuildPath, target, cmakeProjectName, config, buildEnv, sourcePath)
        else:
            for config in configurationList:
                self.RunCMake(tempBuildPath, sourcePath, installPath, cmakeOptionList, buildEnv, config)
                self.Builder.Execute(toolFinder, tempBuildPath, target, cmakeProjectName, config, buildEnv, sourcePath)

    def RunCMake(
        self,
        path: str,
        sourcePath: str,
        cmakeInstallPrefix: str,
        cmakeOptionList: list[str],
        buildEnv: dict[str, str],
        buildVariantConfig: BuildVariantConfig | None = None,
    ) -> None:
        defineCMakeInstallPrefix = f"-DCMAKE_INSTALL_PREFIX={cmakeInstallPrefix}"

        # Add user state
        # self.SaveStateManager.CMakeState.Add(buildVariantConfig)
        # = CMakeAndBuildTaskCMakeSaveState(self.CMakeConfig.CMakeUserArguments)

        defineBuildType = self.__TryGetBuildTypeString(buildVariantConfig)

        self.LogPrint(f"* Running cmake at '{path}' for source '{sourcePath}' with prefix {defineCMakeInstallPrefix} and options {cmakeOptionList}")

        buildCommand = [self.CMakeConfig.CMakeCommand, "-G", self.CMakeConfig.CMakeFinalGeneratorName, defineCMakeInstallPrefix, sourcePath]
        if self.CMakeConfig.EmscriptenEnabled:
            buildCommand.insert(0, self.CMakeConfig.EmscriptenConfigureCommand)

        if defineBuildType is not None:
            buildCommand.append(f"-D{defineBuildType}")

        if len(cmakeOptionList) > 0:
            buildCommand += cmakeOptionList

        self.Log.LogPrintVerbose(4, f"Build commands '{buildCommand}'")

        result = subprocess.call(buildCommand, cwd=path, env=buildEnv)
        if result != 0:
            raise Exception(f"CMake failed {buildCommand}")

    # def __AddToolDependencies()

    def __ApplyPath(self, env: dict[str, str], paths: list[str]) -> None:
        if len(paths) <= 0:
            return
        res = ";{}".format(";".join(paths))
        if "PATH" in env:
            env["PATH"] += res
        else:
            env["PATH"] = res

    def __TryGetBuildTypeString(self, buildVariantConfig: BuildVariantConfig | None) -> str | None:
        if buildVariantConfig is None:
            return None
        buildType = CMakeBuildType.FromBuildVariantConfig(buildVariantConfig)
        return f"CMAKE_BUILD_TYPE={buildType}"

    def __DetermineBuilder(self, generatorName: str, generatorContext: GeneratorContext, buildThreads: int) -> CMakeBuilder:
        if generatorName == CMakeGeneratorName.Android:
            if PlatformUtil.DetectBuildPlatformType() == BuildPlatformType.Windows:
                return CMakeBuilderNinja(generatorContext, buildThreads)
            return CMakeBuilderMake(generatorContext, buildThreads)
        isMSVC = (
            generatorName == CMakeGeneratorName.VisualStudio2015_X64
            or generatorName == CMakeGeneratorName.VisualStudio2017_X64
            or generatorName == CMakeGeneratorName.VisualStudio2019_X64
            or generatorName == CMakeGeneratorName.VisualStudio2022_X64
            or generatorName == CMakeGeneratorName.VisualStudio2022_X64
        )
        # The generic handler does not really apply proper threaded builds, so we use the old one for MSVC
        if generatorContext.CMakeConfig.CMakeVersion < CMakeBuilderGeneric.MINIMUM_VERSION or isMSVC:
            if isMSVC:
                return CMakeBuilderMSBuild(generatorContext, buildThreads)
            if generatorName == CMakeGeneratorName.Ninja:
                return CMakeBuilderNinja(generatorContext, buildThreads, False)
            if generatorName == CMakeGeneratorName.UnixMakeFile:
                return CMakeBuilderMake(generatorContext, buildThreads)
        return CMakeBuilderGeneric(generatorContext, buildThreads)
