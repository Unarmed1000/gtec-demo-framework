#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright (c) 2014 Freescale Semiconductor, Inc.
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

# from typing import Union
import copy
import os
import os.path
from typing import cast

# from FslBuildGen import PackageListUtil
# from FslBuildGen import PackageUtil
from FslBuildGen import IOUtil, PackageConfig, PluginSharedValues, Util
from FslBuildGen.BasicConfig import BasicConfig

# from FslBuildGen.Build.Filter import PackageFilter
from FslBuildGen.Build.Filter import LocalUtil
from FslBuildGen.Config import Config
from FslBuildGen.Context.GeneratorContext import GeneratorContext
from FslBuildGen.Context.PlatformContext import PlatformContext
from FslBuildGen.Context.VariableContext import VariableContext
from FslBuildGen.DataTypes import BuildVariantConfig, FilterMode, GeneratorType, PackageType
from FslBuildGen.Engine.EngineResolveConfig import EngineResolveConfig

# from FslBuildGen.Engine.PackageFlavorName import PackageFlavorName
from FslBuildGen.ErrorHelpManager import ErrorHelpManager
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.ExternalVariantConstraints import ExternalVariantConstraints
from FslBuildGen.Generator import PluginConfig

# from FslBuildGen.Generator.GeneratorCMakeConfig import GeneratorCMakeConfig
from FslBuildGen.Generator.GeneratorPlugin import GenerateContext, GeneratorPlugin
from FslBuildGen.Generator.PluginConfigContext import PluginConfigContext
from FslBuildGen.Log import Log
from FslBuildGen.PackageFile import PackageFile
from FslBuildGen.PackageFilters import PackageFilters
from FslBuildGen.PackageLoader import PackageLoader
from FslBuildGen.PackageManager import PackageManagerFilter
from FslBuildGen.PackageResolver import PackageResolver
from FslBuildGen.Packages.Package import Package
from FslBuildGen.PlatformUtil import PlatformUtil
from FslBuildGen.Tool import ToolAppMain
from FslBuildGen.Tool.LowLevelToolConfig import LowLevelToolConfig
from FslBuildGen.ToolConfig import ToolConfig, ToolConfigPackageConfigurationAddLocationType, ToolConfigPackageConfigurationLocationSetup
from FslBuildGen.ToolConfigPackageRootUtil import ToolConfigPackageRootUtil
from FslBuildGen.ToolMinimalConfig import ToolMinimalConfig
from FslBuildGen.VariableContextHelper import VariableContextHelper
from FslBuildGen.Version import Version
from FslBuildGen.Xml.XmlGenFile import XmlGenFile


def DoGetFiles(
    config: Config, toolMiniConfig: ToolMinimalConfig, currentDir: str, allowRecursiveScan: bool = False, additionalDirs: list[str] | None = None
) -> list[str]:
    """
    :param currentDir: currentDir must be part of a package root
    :param allowRecursiveScan: if True and not a sdk build all subdirectories will be scanned
    :param additionalDirs: extra directories to scan for packages
    """
    if allowRecursiveScan and config.IsSDKBuild:
        config.DoPrintWarning("recursive is ignored for sdk builds")

    allDirs = [currentDir]
    if additionalDirs:
        allDirs.extend(additionalDirs)

    theFiles: list[str] = []
    seen: set[str] = set()
    for scanDir in allDirs:
        if ToolConfigPackageRootUtil.TryFindRootDirectory(toolMiniConfig.RootDirectories, scanDir) is None:
            raise UsageErrorException(f"the folder '{scanDir}' does not reside inside one of the root dirs")
        if not config.IsSDKBuild:
            if allowRecursiveScan:
                for f in IOUtil.FindFileByName(scanDir, config.GenFileName, toolMiniConfig.IgnoreDirectories):
                    if f not in seen:
                        seen.add(f)
                        theFiles.append(f)
            else:
                theFile = IOUtil.Join(scanDir, config.GenFileName)
                if not os.path.isfile(theFile):
                    raise Exception(f"File not found: '{theFile}'")
                if theFile not in seen:
                    seen.add(theFile)
                    theFiles.append(theFile)
    return theFiles


class PackageLoadAndResolveProcess:
    def __init__(self, config: Config, packageLoader: PackageLoader | None = None, plugin: GeneratorPlugin | None = None, writeGraph: bool = False) -> None:
        self.Config = config
        self.Log = config
        self.MarkExternalLibFirstUse = False
        self.SourceFiles: list[str] = []
        self.FoundInputFiles: list[PackageFile] = []
        self.LoadedGenFiles: list[XmlGenFile] = []
        self.Packages: list[Package] = []
        self.IsFullResolve = True
        self.__loadCalled = False
        self.__writeGraph = writeGraph

        if packageLoader is not None:
            if plugin is None:
                raise Exception("A package loader also requires a plugin parameter")
            self.__ExtractFromPackageLoader(packageLoader, plugin)

    def __ExtractFromPackageLoader(self, packageLoader: PackageLoader, plugin: GeneratorPlugin) -> None:
        self.SourceFiles = packageLoader.SourceFiles
        self.FoundInputFiles = packageLoader.FoundInputFiles
        self.LoadedGenFiles = packageLoader.GenFiles
        self.MarkExternalLibFirstUse = plugin.PackageResolveConfig_MarkExternalLibFirstUse
        self.__loadCalled = True

    def Load(self, filePathList: list[str], plugin: GeneratorPlugin, forceImportPackageNames: list[str] | None = None) -> None:
        if self.__loadCalled:
            raise Exception("Load has already been called")
        packageLoader = PackageLoader(self.Config, filePathList, plugin, forceImportPackageNames)
        self.__ExtractFromPackageLoader(packageLoader, plugin)

    def Resolve(
        self,
        platformContext: PlatformContext,
        packageFilters: PackageFilters,
        engineResolveConfig: EngineResolveConfig,
        externalVariantConstraints: ExternalVariantConstraints,
        filterMode: FilterMode,
        autoAddRecipeExternals: bool = True,
        fullResolve: bool = True,
    ) -> list[Package]:
        sourceGenFiles = self.LoadedGenFiles
        if not self.__loadCalled:
            raise Exception("Load has not been called")

        packageManagerFilter = PackageManagerFilter(self.SourceFiles, packageFilters)

        log: Log = self.Config
        configBuildDir = self.Config.GetBuildDir()
        configIsDryRun = self.Config.IsDryRun
        configIgnoreNotSupported = self.Config.IgnoreNotSupported
        configAllowVariantExtension = self.Config.AllowVariantExtension
        configAllowExeDependency = self.Config.AllowExeDependency
        configGroupException = self.Config.GroupException
        toolConfig = self.Config.ToolConfig

        packageResolver = PackageResolver(
            log,
            configBuildDir,
            configIsDryRun,
            configIgnoreNotSupported,
            configAllowVariantExtension,
            configGroupException,
            toolConfig,
            platformContext,
            sourceGenFiles,
            autoAddRecipeExternals,
            fullResolve,
            self.MarkExternalLibFirstUse,
            packageFilters.RecipeFilterManager,
            packageManagerFilter,
            externalVariantConstraints,
            engineResolveConfig,
            self.__writeGraph,
            filterMode,
            configAllowExeDependency,
        )
        self.IsFullResolve = fullResolve
        self.Packages = packageResolver.Packages
        return self.Packages


def DoGetPackages(
    generatorContext: GeneratorContext,
    config: Config,
    filePathList: list[str],
    packageFilters: PackageFilters,
    autoAddRecipeExternals: bool = True,
    forceImportPackageNames: list[str] | None = None,
    engineResolveConfig: EngineResolveConfig | None = None,
) -> list[Package]:
    if engineResolveConfig is None:
        engineResolveConfig = EngineResolveConfig.CreateDefault()
    process = PackageLoadAndResolveProcess(config)
    process.Load(filePathList, generatorContext.Platform, forceImportPackageNames)
    process.Resolve(
        generatorContext, packageFilters, engineResolveConfig, config.VariantConstraints, FilterMode.TrimUnrequestedPackages, autoAddRecipeExternals
    )
    return process.Packages


def __ResolveAndGenerate(
    config: Config,
    variableContext: VariableContext,
    errorHelpManager: ErrorHelpManager,
    platformGeneratorPlugin: GeneratorPlugin,
    packageLoader: PackageLoader,
    packageFilters: PackageFilters,
    engineResolveConfig: EngineResolveConfig,
    isSDKBuild: bool,
    writeGraph: bool,
    filterMode: FilterMode,
) -> list[Package]:
    generatorContext = GeneratorContext(
        config, errorHelpManager, packageFilters.RecipeFilterManager, config.ToolConfig.Experimental, platformGeneratorPlugin, variableContext
    )

    process = PackageLoadAndResolveProcess(config, packageLoader, platformGeneratorPlugin, writeGraph=writeGraph)
    process.Resolve(generatorContext, packageFilters, engineResolveConfig, config.VariantConstraints, filterMode)

    if not isSDKBuild:
        for package in process.Packages:
            if not package.ResolvedPlatformSupported and package.Type != PackageType.TopLevel:
                notSupported = LocalUtil.BuildListOfDirectlyNotSupported(package)
                notSupportedNames = Util.ExtractNames(notSupported)
                config.DoPrintWarning(f"{package.Name} was marked as not supported on this platform by package: {notSupportedNames}")

    return platformGeneratorPlugin.Generate(GenerateContext(generatorContext, config, process.Packages, config.VariantConstraints))


def DoGenerateBuildFiles(
    pluginConfigContext: PluginConfigContext,
    config: Config,
    variableContext: VariableContext,
    errorHelpManager: ErrorHelpManager,
    files: list[str],
    platformGeneratorPlugin: GeneratorPlugin,
    packageFilters: PackageFilters,
    writeGraph: bool = False,
) -> list[Package]:
    config.LogPrint("- Generating build files")

    isSDKBuild = len(files) <= 0
    packageLoader = PackageLoader(config, files, platformGeneratorPlugin)
    engineResolveConfig = EngineResolveConfig.CreateDefault()
    engineResolveConfig = EngineResolveConfig.CreateDefaultFlavor()
    return __ResolveAndGenerate(
        config,
        variableContext,
        errorHelpManager,
        platformGeneratorPlugin,
        packageLoader,
        packageFilters,
        engineResolveConfig,
        isSDKBuild,
        writeGraph,
        FilterMode.TrimUnrequestedPackages,
    )


def DoGenerateBuildFilesNoAll(
    config: Config,
    variableContext: VariableContext,
    errorHelpManager: ErrorHelpManager,
    files: list[str],
    platformGeneratorPlugin: GeneratorPlugin,
    packageFilters: PackageFilters,
) -> list[Package]:
    config.LogPrint("- Generating build files")
    isSDKBuild = len(files) <= 0
    packageLoader = PackageLoader(config, files, platformGeneratorPlugin)
    engineResolveConfig = EngineResolveConfig.CreateDefault()
    return __ResolveAndGenerate(
        config,
        variableContext,
        errorHelpManager,
        platformGeneratorPlugin,
        packageLoader,
        packageFilters,
        engineResolveConfig,
        isSDKBuild,
        False,
        FilterMode.TrimUnrequestedPackages,
    )


def DoGenerateBuildFilesNow(
    pluginConfigContext: PluginConfigContext,
    config: Config,
    variableContext: VariableContext,
    errorHelpManager: ErrorHelpManager,
    files: list[str],
    platformGeneratorPlugin: GeneratorPlugin,
    packageFilters: PackageFilters,
    engineResolveConfig: EngineResolveConfig,
    filterMode: FilterMode,
) -> tuple[list[Package], GeneratorPlugin] | None:
    config.LogPrint("- Generating build files")

    isSDKBuild = len(files) <= 0
    packageLoader = PackageLoader(config, files, platformGeneratorPlugin)
    res: tuple[list[Package], GeneratorPlugin] | None = None
    for entry in pluginConfigContext.GetGeneratorPlugins():
        if entry.PlatformName.lower() == platformGeneratorPlugin.OriginalPlatformId and (not entry.InDevelopment):
            packages = __ResolveAndGenerate(
                config,
                variableContext,
                errorHelpManager,
                entry,
                copy.deepcopy(packageLoader),
                packageFilters,
                engineResolveConfig,
                isSDKBuild,
                False,
                filterMode,
            )
            res = (packages, entry)
    return res


def ToUnitTestPath(config: Config, path: str) -> str:
    if config.TestPath is None:
        raise Exception("config.TestPath not configured")
    return IOUtil.Join(config.TestPath, path)


def ToUnitTestPaths(config: Config, paths: list[str]) -> list[str]:
    res = []
    for path in paths:
        res.append(ToUnitTestPath(config, path))
    return res


def ToolConfigPackageConfigurationLocationSetupToUnitTestPaths(
    config: Config, location: ToolConfigPackageConfigurationLocationSetup
) -> ToolConfigPackageConfigurationLocationSetup:
    newName = ToUnitTestPath(config, location.Name)
    return ToolConfigPackageConfigurationLocationSetup(newName, location.ScanMethod, location.Blacklist)


def CustomUnitTestRootsToUnitTestPaths(config: Config, paths: ToolConfigPackageConfigurationAddLocationType) -> ToolConfigPackageConfigurationAddLocationType:
    if isinstance(paths, str):
        return ToUnitTestPath(config, paths)
    elif isinstance(paths, ToolConfigPackageConfigurationLocationSetup):
        return ToolConfigPackageConfigurationLocationSetupToUnitTestPaths(config, paths)

    if not isinstance(paths, list):
        raise Exception("Not supported")

    if len(paths) <= 0:
        return paths

    if isinstance(paths[0], str):
        paths1 = cast(list[str], paths)
        res1 = []
        for path1 in paths1:
            res1.append(ToUnitTestPath(config, path1))
        return res1

    paths2 = cast(list[ToolConfigPackageConfigurationLocationSetup], paths)
    res2 = []
    for path2 in paths2:
        res2.append(ToolConfigPackageConfigurationLocationSetupToUnitTestPaths(config, path2))
    return res2


def GetDefaultConfigForTest(
    enableTestMode: bool = False, customUnitTestRoots: list[str] | None = None, externalConstraints: ExternalVariantConstraints | None = None
) -> Config:
    strToolAppTitle = "UnitTest"
    log = Log(strToolAppTitle, 0)
    currentDir = IOUtil.GetEnvironmentVariableForDirectory("FSL_GRAPHICS_INTERNAL")
    basicConfig = BasicConfig(log)
    localToolConfig = LowLevelToolConfig(log.Verbosity, False, False, False, False, currentDir, [], False)
    projectRootConfig = ToolAppMain.GetProjectRootConfig(localToolConfig, basicConfig, currentDir)
    buildPlatformType = PlatformUtil.DetectBuildPlatformType()
    toolConfig = ToolConfig(localToolConfig, buildPlatformType, Version(1, 3, 3, 7), basicConfig, projectRootConfig.ToolConfigFile, projectRootConfig)
    config = Config(log, toolConfig, PluginSharedValues.TYPE_UNIT_TEST, externalConstraints, True)
    config.ForceDisableAllWrite()
    if enableTestMode:
        config.SetTestMode()
    if customUnitTestRoots is not None:
        TEST_AddPackageRoots(config, customUnitTestRoots, True)
    return config


# def __GetTestGeneratorCMakeConfig() -> GeneratorCMakeConfig:
#    generatorCMakeConfig = GeneratorCMakeConfig()
#    return generatorCMakeConfig


def __TestGenerateBuildFilesAllPlatforms(
    config: Config, files: list[str], engineResolveConfig: EngineResolveConfig | None, variableContext: VariableContext | None = None
) -> dict[str, list[Package]]:
    if engineResolveConfig is None:
        engineResolveConfig = EngineResolveConfig.CreateDefault()
    if variableContext is None:
        variableContext = VariableContextHelper.CreateDefault(config.ToolConfig)
    res: dict[str, list[Package]] = {}
    for platformId in PackageConfig.APPROVED_PLATFORM_NAMES:
        errorHelpManager = ErrorHelpManager()
        packageFilters = PackageFilters()
        log: Log = config
        # generatorCMakeConfig = __GetTestGeneratorCMakeConfig()
        pluginConfigContext = PluginConfig.InitPluginConfigContext(log, config.ToolConfig.ToolVersion, allowDevelopmentPlugins=True)
        pluginConfigContext.SetVSVersion(str(config.ToolConfig.GetVisualStudioDefaultVersion()))

        buildVariantConfig = BuildVariantConfig.Debug
        platform = pluginConfigContext.GetGeneratorPluginById(
            platformId,
            GeneratorType.Default,
            buildVariantConfig,
            variableContext.UserSetVariables,
            config.ToolConfig.DefaultPackageLanguage,
            config.ToolConfig.CMakeConfiguration,
            None,
            False,
        )
        resultTuple = DoGenerateBuildFilesNow(
            pluginConfigContext, config, variableContext, errorHelpManager, files, platform, packageFilters, engineResolveConfig, FilterMode.Disabled
        )
        if resultTuple is not None:
            res[platformId] = resultTuple[0]
    return res


def __TestGetPackageLoader(config: Config, files: list[str], platformId: str, variableContext: VariableContext | None = None) -> PackageLoader:
    if variableContext is None:
        variableContext = VariableContextHelper.CreateDefault(config.ToolConfig)
    # packageFilters = PackageFilters()
    # generatorCMakeConfig = __GetTestGeneratorCMakeConfig()
    log: Log = config
    pluginConfigContext = PluginConfig.InitPluginConfigContext(log, config.ToolConfig.ToolVersion, allowDevelopmentPlugins=True)
    buildVariantConfig = BuildVariantConfig.Debug
    platformGeneratorPlugin = pluginConfigContext.GetGeneratorPluginById(
        platformId,
        GeneratorType.Default,
        buildVariantConfig,
        variableContext.UserSetVariables,
        config.ToolConfig.DefaultPackageLanguage,
        config.ToolConfig.CMakeConfiguration,
        None,
        False,
    )
    return PackageLoader(config, files, platformGeneratorPlugin)


def TEST_AddPackageRoots(config: Config, customUnitTestRoots: ToolConfigPackageConfigurationAddLocationType, replaceExistingLocations: bool = False) -> None:
    unitTestRootList = CustomUnitTestRootsToUnitTestPaths(config, customUnitTestRoots)
    activePackageConfiguration = config.ToolConfig.PackageConfiguration[PluginSharedValues.TYPE_UNIT_TEST]
    if replaceExistingLocations:
        activePackageConfiguration.ClearLocations("$(FSL_GRAPHICS_SDK)/ThirdParty")
    activePackageConfiguration.AddLocations(unitTestRootList)


def SimpleTestHookOneFile(theFile: str, engineResolveConfig: EngineResolveConfig | None = None) -> dict[str, list[Package]]:
    config = GetDefaultConfigForTest()
    theFile = ToUnitTestPath(config, theFile)
    return __TestGenerateBuildFilesAllPlatforms(config, [theFile], engineResolveConfig)


def SimpleTestHookFiles(theFiles: list[str], engineResolveConfig: EngineResolveConfig | None = None) -> dict[str, list[Package]]:
    config = GetDefaultConfigForTest()
    theFiles = ToUnitTestPaths(config, theFiles)
    return __TestGenerateBuildFilesAllPlatforms(config, theFiles, engineResolveConfig)


def SimpleTestHookFilesWithCustomPackageRoot(
    theFiles: list[str], customUnitTestRoots: list[str], engineResolveConfig: EngineResolveConfig | None = None
) -> dict[str, list[Package]]:
    config = GetDefaultConfigForTest()
    TEST_AddPackageRoots(config, customUnitTestRoots)
    theFiles = ToUnitTestPaths(config, theFiles)
    return __TestGenerateBuildFilesAllPlatforms(config, theFiles, engineResolveConfig)


def SimpleTestHookOneFileEx(theFile: str, config: Config, engineResolveConfig: EngineResolveConfig | None = None) -> dict[str, list[Package]]:
    config.ForceDisableAllWrite()
    theFile = ToUnitTestPath(config, theFile)
    return __TestGenerateBuildFilesAllPlatforms(config, [theFile], engineResolveConfig)


def SimpleTestHookFilesEx(theFiles: list[str], config: Config, engineResolveConfig: EngineResolveConfig | None = None) -> dict[str, list[Package]]:
    config.ForceDisableAllWrite()
    theFiles = ToUnitTestPaths(config, theFiles)
    return __TestGenerateBuildFilesAllPlatforms(config, theFiles, engineResolveConfig)


def SimpleTestHookGetPackageLoaderOneFileEx(file: str, config: Config) -> list[PackageLoader]:
    config.ForceDisableAllWrite()
    theFiles = ToUnitTestPaths(config, [file])
    res: list[PackageLoader] = []
    for platformId in PackageConfig.APPROVED_PLATFORM_NAMES:
        res.append(__TestGetPackageLoader(config, theFiles, platformId))
    return res
