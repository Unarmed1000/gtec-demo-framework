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

import os
import os.path

from FslBuildGen import IOUtil, PathUtil, Util
from FslBuildGen.BasicConfig import BasicConfig
from FslBuildGen.BuildConfig.BuildDocConfiguration import BuildDocConfiguration
from FslBuildGen.BuildConfig.BuildDocConfigurationRequirement import BuildDocConfigurationRequirement
from FslBuildGen.BuildConfig.ClangFormatConfiguration import ClangFormatConfiguration
from FslBuildGen.BuildConfig.ClangTidyConfiguration import ClangTidyConfiguration
from FslBuildGen.BuildConfig.ClangTidyPlatform import ClangTidyPlatform
from FslBuildGen.BuildConfig.ClangTidyPlatformCompiler import ClangTidyPlatformCompiler
from FslBuildGen.BuildConfig.ClangTidyPlatformDefines import ClangTidyPlatformDefines
from FslBuildGen.BuildConfig.CMakeConfiguration import CMakeConfiguration
from FslBuildGen.BuildConfig.CMakeConfigurationPlatform import CMakeConfigurationPlatform
from FslBuildGen.BuildConfig.DotnetFormatConfiguration import DotnetFormatConfiguration
from FslBuildGen.CMakeIgnoreDirUtil import CMakeIgnoreDirUtil
from FslBuildGen.CMakeUtil import CMakeUtil, CMakeVersion
from FslBuildGen.DataTypes import BuildPlatformType, CompilerNames, MagicStrings, PackageRequirementTypeString, VisualStudioVersion
from FslBuildGen.Exceptions import (
    DuplicatedConfigBasePackage,
    DuplicatedConfigContentBuilder,
    DuplicatedConfigPackageLocation,
    DuplicatedConfigRootPath,
    DuplicatedNewProjectTemplatesRootPath,
    UsageErrorException,
)
from FslBuildGen.Generator.GeneratorCMakeConfig import GeneratorCMakeConfig
from FslBuildGen.GitUtil import GitUtil
from FslBuildGen.Log import Log
from FslBuildGen.Tool.LowLevelToolConfig import LowLevelToolConfig
from FslBuildGen.ToolConfigBasePackage import ToolConfigBasePackage
from FslBuildGen.ToolConfigExperimental import ToolConfigExperimental
from FslBuildGen.ToolConfigPackageRootUtil import ToolConfigPackageRootUtil
from FslBuildGen.ToolConfigProjectContext import ToolConfigProjectContext
from FslBuildGen.ToolConfigProjectInfo import ToolConfigProjectInfo
from FslBuildGen.ToolConfigRootDirectory import ToolConfigRootDirectory
from FslBuildGen.ToolMinimalConfig import ToolMinimalConfig
from FslBuildGen.Vars.VariableProcessor import VariableProcessor
from FslBuildGen.Version import Version
from FslBuildGen.Xml.Exceptions import XmlDuplicatedCompilerConfigurationException, XmlUnsupportedCompilerVersionException
from FslBuildGen.Xml.Project.XmlBuildDocConfiguration import XmlBuildDocConfiguration
from FslBuildGen.Xml.Project.XmlClangTidyConfiguration import XmlClangTidyConfiguration
from FslBuildGen.Xml.Project.XmlClangTidyPlatform import XmlClangTidyPlatform
from FslBuildGen.Xml.Project.XmlClangTidyPlatformCompiler import XmlClangTidyPlatformCompiler
from FslBuildGen.Xml.Project.XmlClangTidyPlatformDefines import XmlClangTidyPlatformDefines
from FslBuildGen.Xml.Project.XmlClangTidyPlatformStrictChecks import XmlClangTidyPlatformStrictChecks
from FslBuildGen.Xml.Project.XmlCMakeConfiguration import XmlCMakeConfiguration
from FslBuildGen.Xml.Project.XmlProjectRootConfigFile import (
    XmlClangFormatConfiguration,
    XmlConfigCompilerConfiguration,
    XmlConfigFileAddBasePackage,
    XmlConfigFileAddRootDirectory,
    XmlDotnetFormatConfiguration,
    XmlExperimental,
    XmlProjectRootConfigFile,
)
from FslBuildGen.Xml.ToolConfig.XmlConfigFileAddNewProjectTemplatesRootDirectory import XmlConfigFileAddNewProjectTemplatesRootDirectory
from FslBuildGen.Xml.ToolConfig.XmlConfigPackageConfiguration import XmlConfigPackageConfiguration
from FslBuildGen.Xml.ToolConfig.XmlConfigPackageLocation import FakeXmlConfigPackageLocation, XmlConfigPackageLocation
from FslBuildGen.Xml.XmlToolConfigFile import (
    XmlConfigContentBuilder,
    XmlConfigContentBuilderConfiguration,
    XmlConfigFileAddTemplateImportDirectory,
    XmlConfigFileTemplateFolder,
    XmlToolConfigFile,
)


class ToolConfigCompilerConfiguration:
    def __init__(self, basicConfig: BasicConfig, basedUponXML: XmlConfigCompilerConfiguration) -> None:
        super().__init__()
        self.BasedOn = basedUponXML
        self.Name = self.BasedOn.Name
        self.Id = self.BasedOn.Id
        self.Platform = self.BasedOn.Platform
        defaultVersion = VisualStudioVersion.TryParse(self.BasedOn.DefaultVersion)
        if defaultVersion is None:
            raise XmlUnsupportedCompilerVersionException(
                self.BasedOn.XMLElement, self.BasedOn.Name, self.BasedOn.DefaultVersion, ", ".join(str(x) for x in VisualStudioVersion.AllEntries)
            )
        self.DefaultVersion = defaultVersion


class ToolConfigTemplateFolder:
    def __init__(self, basicConfig: BasicConfig, basedUponXML: XmlConfigFileTemplateFolder) -> None:
        super().__init__()
        self.BasedOn = basedUponXML
        self.Name = self.BasedOn.Name

        variableProcessor = VariableProcessor(basicConfig)

        self.ResolvedPath = variableProcessor.ResolveAbsolutePathWithLeadingEnvironmentVariablePathAsDir(self.Name)


class NewProjectTemplateRootDirectory:
    def __init__(self, basicConfig: BasicConfig, basedUponXML: XmlConfigFileAddNewProjectTemplatesRootDirectory) -> None:
        super().__init__()
        self.BasedOn = basedUponXML
        self.Id = basedUponXML.Id
        self.Name = basedUponXML.Name
        self.DynamicName = basedUponXML.Name

        variableProcessor = VariableProcessor(basicConfig)

        # NOTE: workaround Union of tuples not being iterable bug in mypy https://github.com/python/mypy/issues/1575
        tupleResult = variableProcessor.TryExtractLeadingEnvironmentVariableNameAndPath(self.DynamicName, True)
        env = tupleResult[0]
        remainingPath = tupleResult[1]
        if env is None:
            raise Exception(f"Root dirs are expected to contain environment variables '{self.DynamicName}'")
        remainingPath = remainingPath if remainingPath is not None else ""

        resolvedPath = IOUtil.GetEnvironmentVariableForDirectory(env) + remainingPath
        self.BashName = f"${env}{remainingPath}"
        self.DosName = f"%{env}%{remainingPath}"
        self.ResolvedPath = IOUtil.ToUnixStylePath(resolvedPath)
        self.ResolvedPathEx = f"{self.ResolvedPath}/" if len(self.ResolvedPath) > 0 else ""
        self.__EnvironmentVariableName = env


class ToolConfigDirectory:
    def __init__(self, basicConfig: BasicConfig, basedUponXML: XmlConfigFileAddTemplateImportDirectory) -> None:
        super().__init__()

        self.BasedOn = basedUponXML
        self.Name = self.BasedOn.Name

        variableProcessor = VariableProcessor(basicConfig)

        # NOTE: workaround Union of tuples not being iterable bug in mypy https://github.com/python/mypy/issues/1575
        tupleResult = variableProcessor.TrySplitLeadingEnvironmentVariablesNameAndPath(self.Name)
        envName = tupleResult[0]
        rest = tupleResult[1]
        if envName is None:
            raise Exception("Template import dirs are expected to contain environment variables")
        rest = rest if rest is not None else ""

        self.DecodedName = envName
        self.BashName = IOUtil.Join("$" + self.DecodedName, rest)
        self.DosName = IOUtil.Join("%" + self.DecodedName + "%", rest)
        self.ResolvedPath = IOUtil.Join(IOUtil.GetEnvironmentVariableForDirectory(self.DecodedName), rest)
        self.ResolvedPathEx = f"{self.ResolvedPath}/" if len(self.ResolvedPath) > 0 else ""


# TODO: improve interface, dont allow so many None (remove None from rootDirs and projectRootDirectory)
class ToolConfigLocation:
    def __init__(
        self,
        basicConfig: BasicConfig,
        rootDirs: list[ToolConfigRootDirectory] | None,
        basedUponXML: XmlConfigPackageLocation,
        projectRootDirectory: str | None,
        resolvedPath: str | None = None,
    ) -> None:
        super().__init__()
        if (rootDirs is None or projectRootDirectory is None) and (rootDirs is not None or projectRootDirectory is not None):
            raise Exception("When rootDirs is none, then the projectRootDirectory must be none")

        # Do some basic validation of the path
        PathUtil.ValidateIsNormalizedPath(basedUponXML.Name, "Location")

        self.BasedOn = basedUponXML
        self.Id = basedUponXML.Id
        self.Name = basedUponXML.Name

        if resolvedPath is not None:
            self.ResolvedPath = IOUtil.NormalizePath(resolvedPath)
        else:
            if rootDirs is None or projectRootDirectory is None:
                raise Exception("When resolvedPath is None then rootDirs and projectRootDirectory can not be None")
            self.ResolvedPath = self.__ResolvePath(basicConfig, rootDirs, self.Name, projectRootDirectory)
        self.ResolvedPathEx = f"{self.ResolvedPath}/" if len(self.ResolvedPath) > 0 else ""
        self.ScanMethod = basedUponXML.ScanMethod

    def __ResolvePath(self, basicConfig: BasicConfig, rootDirs: list[ToolConfigRootDirectory], entryName: str, projectRootDirectory: str) -> str:
        rootDir = self.__LocateRootDir(basicConfig, rootDirs, entryName, projectRootDirectory)
        return entryName.replace(rootDir.Name, rootDir.ResolvedPath)

    def __LocateRootDir(
        self, basicConfig: BasicConfig, rootDirs: list[ToolConfigRootDirectory], entryName: str, projectRootDirectory: str
    ) -> ToolConfigRootDirectory:
        if projectRootDirectory is None or not entryName.startswith(MagicStrings.ProjectRoot):
            for rootDir in rootDirs:
                if entryName.startswith(rootDir.Name):
                    return rootDir
        else:
            # Lets try to locate a root directory which the project is based in,
            # then use it to dynamically add a new allowed root directory based on the project file location
            for rootDir in rootDirs:
                if projectRootDirectory == rootDir.ResolvedPath:
                    return ToolConfigRootDirectory(basicConfig, None, rootDir.ProjectId, rootDir, MagicStrings.ProjectRoot, rootDir.DynamicName)
                elif projectRootDirectory.startswith(rootDir.ResolvedPathEx):
                    dynamicRootDir = projectRootDirectory[len(rootDir.ResolvedPathEx) :]
                    dynamicRootDir = f"{rootDir.Name}/{dynamicRootDir}"
                    return ToolConfigRootDirectory(basicConfig, None, rootDir.ProjectId, rootDir, MagicStrings.ProjectRoot, dynamicRootDir)
        raise Exception("Path '{}' is not based on one of the valid root directories {}".format(entryName, ", ".join(Util.ExtractNames(rootDirs))))


class ToolConfigPackageLocationBlacklistEntry:
    def __init__(self, sourceRootPath: str, relativePath: str) -> None:
        self.RootDirPath = IOUtil.NormalizePath(sourceRootPath)
        self.RelativeDirPath = IOUtil.NormalizePath(relativePath)
        self.AbsoluteDirPath = IOUtil.Join(sourceRootPath, relativePath)


# TODO: improve interface, dont allow so many None (remove None from rootDirs and projectRootDirectory)
class ToolConfigPackageLocation(ToolConfigLocation):
    def __init__(
        self,
        basicConfig: BasicConfig,
        rootDirs: list[ToolConfigRootDirectory] | None,
        basedUponXML: XmlConfigPackageLocation,
        projectRootDirectory: str | None,
        resolvedPath: str | None = None,
    ) -> None:
        super().__init__(basicConfig, rootDirs, basedUponXML, projectRootDirectory, resolvedPath)
        self.Blacklist = [ToolConfigPackageLocationBlacklistEntry(self.ResolvedPath, entry.Name) for entry in basedUponXML.Blacklist]


class ToolConfigPackageConfigurationLocationSetup:
    def __init__(self, name: str, scanMethod: int | None = None, blacklist: list[str] | None = None) -> None:
        self.Name = name
        self.ScanMethod = scanMethod
        self.Blacklist = blacklist


ToolConfigPackageConfigurationAddLocationType = (
    str | ToolConfigPackageConfigurationLocationSetup | list[str] | list[ToolConfigPackageConfigurationLocationSetup]
)


class ToolConfigPackageConfiguration:
    def __init__(
        self,
        basicConfig: BasicConfig,
        rootDirs: list[ToolConfigRootDirectory],
        basedUponXML: XmlConfigPackageConfiguration,
        configFileName: str,
        projectRootDirectory: str,
    ) -> None:
        super().__init__()
        self.__basicConfig = basicConfig
        self.BasedOn = basedUponXML
        self.Name = basedUponXML.Name
        self.Preload = basedUponXML.Preload
        self.Locations = self.__ResolveLocations(basicConfig, rootDirs, basedUponXML.Locations, configFileName, projectRootDirectory)

    def ClearLocations(self, keep: str) -> None:
        oldLocations = self.Locations
        self.Locations = []
        for entry in oldLocations:
            if entry.Name == keep:
                self.Locations.append(entry)

    def AddLocations(self, newRootLocations: ToolConfigPackageConfigurationAddLocationType) -> None:
        # done in two steps to make mypy happy
        if isinstance(newRootLocations, str):
            newRootLocations = [newRootLocations]
        if isinstance(newRootLocations, ToolConfigPackageConfigurationLocationSetup):
            newRootLocations = [newRootLocations]

        for rootLocation in newRootLocations:
            if isinstance(rootLocation, str):
                resolvedPath = rootLocation
                fakeXml = FakeXmlConfigPackageLocation(self.__basicConfig, rootLocation)
            elif isinstance(rootLocation, ToolConfigPackageConfigurationLocationSetup):
                resolvedPath = rootLocation.Name
                fakeXml = FakeXmlConfigPackageLocation(self.__basicConfig, rootLocation.Name, rootLocation.ScanMethod, rootLocation.Blacklist)
            else:
                raise Exception("Unsupported type")
            self.Locations.append(ToolConfigPackageLocation(self.__basicConfig, None, fakeXml, None, resolvedPath))

    def __ResolveLocations(
        self,
        basicConfig: BasicConfig,
        rootDirs: list[ToolConfigRootDirectory],
        locations: list[XmlConfigPackageLocation],
        configFileName: str,
        projectRootDirectory: str,
    ) -> list[ToolConfigPackageLocation]:
        # Check for unique names and
        # convert to a ToolConfigPackageLocation list
        res = []  # List[ToolConfigPackageLocation]
        uniqueLocationIds: set[str] = set()
        for location in locations:
            if location.Id not in uniqueLocationIds:
                uniqueLocationIds.add(location.Id)
                packageLocation = ToolConfigPackageLocation(basicConfig, rootDirs, location, projectRootDirectory)
                res.append(packageLocation)
            else:
                raise DuplicatedConfigPackageLocation(location.Name, configFileName)

        # We sort it so that the longest paths come first meaning we will always find the most exact match first
        # if searching from the front to the end of the list and comparing to 'startswith'
        res.sort(key=lambda s: -len(s.ResolvedPath))
        return res


class ToolContentBuilder:
    def __init__(self, basedUponXML: XmlConfigContentBuilder) -> None:
        super().__init__()
        self.BasedOn = basedUponXML
        self.Name = basedUponXML.Name
        self.Executable = basedUponXML.Executable
        self.Parameters = basedUponXML.Parameters
        self.FeatureRequirements = basedUponXML.FeatureRequirements
        self.DefaultExtensions = basedUponXML.DefaultExtensions
        self.Description = basedUponXML.Description


class ToolConfigContentBuilderConfiguration:
    def __init__(self, basedUponXML: XmlConfigContentBuilderConfiguration, configFileName: str) -> None:
        super().__init__()
        self.BasedOn = basedUponXML
        self.ContentBuilders = self.__ResolveContentBuilders(basedUponXML.ContentBuilders, configFileName) if basedUponXML else []

    def __ResolveContentBuilders(self, contentBuilders: list[XmlConfigContentBuilder], configFileName: str) -> list[ToolContentBuilder]:
        uniqueNames: set[str] = set()
        res: list[ToolContentBuilder] = []
        for contentBuilder in contentBuilders:
            newContentBuilder = ToolContentBuilder(contentBuilder)
            if newContentBuilder.Name not in uniqueNames:
                uniqueNames.add(newContentBuilder.Name)
                res.append(newContentBuilder)
            else:
                raise DuplicatedConfigContentBuilder(newContentBuilder.Name, configFileName)
        return res


class ToolConfig:
    def __init__(
        self,
        lowLevelToolConfig: LowLevelToolConfig,
        buildPlatformType: BuildPlatformType,
        toolVersion: Version,
        basicConfig: BasicConfig,
        filename: str,
        projectRootConfig: XmlProjectRootConfigFile,
    ) -> None:
        super().__init__()
        basedUponXML = XmlToolConfigFile(basicConfig, filename, projectRootConfig)
        self.LowLevelToolConfig = lowLevelToolConfig
        self.ToolVersion = toolVersion
        self.BasedOn = basedUponXML
        self.GenFileName = basedUponXML.GenFileName.Name
        self.RootDirectories = self.__ResolveRootDirectories(basicConfig, basedUponXML.RootDirectories, filename)
        self.TemplateImportDirectories = self.__ResolveDirectories(basicConfig, basedUponXML.TemplateImportDirectories)
        self.PackageConfiguration = self.__ResolvePackageConfiguration(
            basicConfig, self.RootDirectories, basedUponXML.PackageConfiguration, filename, projectRootConfig.RootDirectory
        )
        self.TemplateFolder = ToolConfigTemplateFolder(basicConfig, basedUponXML.TemplateFolder)
        self.NewProjectTemplateRootDirectories = self.__ResolveNewProjectTemplateRootDirectories(basicConfig, basedUponXML.NewProjectTemplateRootDirectories)
        self.ContentBuilderConfiguration = (
            ToolConfigContentBuilderConfiguration(basedUponXML.ContentBuilderConfiguration, filename)
            if basedUponXML.ContentBuilderConfiguration is not None
            else None
        )
        self.UnitTestPath = self.__TryResolveUnitTestPath()
        self.DefaultPackageLanguage = projectRootConfig.DefaultPackageLanguage
        self.DefaultCompany = projectRootConfig.DefaultCompany
        self.RequirePackageCreationYear = projectRootConfig.RequirePackageCreationYear
        self.ProjectRootConfig = projectRootConfig
        self.ProjectInfo = self.__GenerateProjectInfo(basicConfig, buildPlatformType, projectRootConfig, lowLevelToolConfig.NoGitHash)
        self.BuildDocConfiguration = self.__TryGetBuildDocConfiguration(basedUponXML.BuildDocConfiguration)
        self.CMakeConfiguration = self.__GetCMakeConfiguration(basedUponXML.CMakeConfiguration)
        self.ClangFormatConfiguration = self.__TryGetClangFormatConfiguration(
            basedUponXML.ClangFormatConfiguration, self.CMakeConfiguration.NinjaRecipePackageName
        )
        self.DotnetFormatConfiguration = self.__TryGetDotnetFormatConfiguration(
            basedUponXML.DotnetFormatConfiguration, self.CMakeConfiguration.NinjaRecipePackageName
        )
        self.ClangTidyConfiguration = self.__TryGetClangTidyConfiguration(basedUponXML.ClangTidyConfiguration, self.CMakeConfiguration.NinjaRecipePackageName)
        self.CompilerConfigurationDict = self.__ProcessCompilerConfiguration(basicConfig, basedUponXML.CompilerConfiguration)
        self.RequirementTypes = [PackageRequirementTypeString.Extension, PackageRequirementTypeString.Feature]
        self.Experimental: ToolConfigExperimental | None = self.__ResolveExperimental(
            basicConfig, self.RootDirectories, basedUponXML.Experimental, filename, projectRootConfig.RootDirectory
        )

        if buildPlatformType == BuildPlatformType.Windows:
            self.__ResolvedLegacyToCurrentOSPathMethod = self.TryLegacyToDosPath
            self.__ResolvedLegacyToCurrentOSPathDirectConversionMethod = self.TryLegacyToDosPathDirectConversion
            self.__ResolvedToCurrentOSPathMethod = self.ToDosPath
            self.__ResolvedToCurrentOSPathDirectConversionMethod = self.ToDosPathDirectConversion
        else:
            self.__ResolvedLegacyToCurrentOSPathMethod = self.TryLegacyToBashPath
            self.__ResolvedLegacyToCurrentOSPathDirectConversionMethod = self.TryLegacyToBashPathDirectConversion
            self.__ResolvedToCurrentOSPathMethod = self.ToBashPath
            self.__ResolvedToCurrentOSPathDirectConversionMethod = self.ToBashPathDirectConversion

    def GetMinimalConfig(self, cmakeConfig: GeneratorCMakeConfig | None) -> ToolMinimalConfig:
        ignoreDirectories: list[str] = []
        # ignore the template import directory
        for templateImport in self.TemplateImportDirectories:
            ignoreDirectories.append(templateImport.ResolvedPath)
        # ignore the NewProjectTemplateRootDirectories
        for newProjectTemplate in self.NewProjectTemplateRootDirectories:
            ignoreDirectories.append(newProjectTemplate.ResolvedPath)

        if cmakeConfig is not None:
            ignoreDirectories += CMakeIgnoreDirUtil.GetIgnoreDirs(self.ProjectInfo, cmakeConfig, self.CMakeConfiguration.DefaultBuildDir)

        return ToolMinimalConfig(self.RootDirectories, ignoreDirectories)

    def __TryGetBuildDocConfiguration(self, configList: list[XmlBuildDocConfiguration]) -> BuildDocConfiguration:
        requirementList: list[BuildDocConfigurationRequirement] = []
        if len(configList) < 1:
            return BuildDocConfiguration(requirementList)
        config = configList[0]
        for requirement in config.Requirements:
            requirementList.append(BuildDocConfigurationRequirement(requirement.Name, requirement.Skip))
        return BuildDocConfiguration(requirementList)

    def __TryGetClangFormatConfiguration(self, configList: list[XmlClangFormatConfiguration], ninjaRecipePackageName: str) -> ClangFormatConfiguration | None:
        if len(configList) < 1:
            return None
        config = configList[0]
        return ClangFormatConfiguration(config.FileExtensions, config.Recipe, ninjaRecipePackageName)

    def __TryGetDotnetFormatConfiguration(
        self, configList: list[XmlDotnetFormatConfiguration], ninjaRecipePackageName: str
    ) -> DotnetFormatConfiguration | None:
        if len(configList) < 1:
            return None
        config = configList[0]
        return DotnetFormatConfiguration(config.FileExtensions, config.Recipe, ninjaRecipePackageName)

    def __TryGetClangTidyConfiguration(self, configList: list[XmlClangTidyConfiguration], ninjaRecipePackageName: str) -> ClangTidyConfiguration | None:
        if len(configList) < 1:
            return None
        config = configList[0]
        platforms = self.__GetClangTidyPlatforms(config.Platforms)
        clangConfig = ClangTidyConfiguration(config.FileExtensions, config.ClangRecipe, config.ClangTidyRecipe, ninjaRecipePackageName, platforms)
        allPlatformName = "all"
        if allPlatformName in clangConfig.PlatformDict:
            # append the all configuration to all other configurations
            allPlatform = clangConfig.PlatformDict[allPlatformName]
            for platform in clangConfig.PlatformDict.values():
                if platform != allPlatform:
                    platform.Merge(allPlatform)
        return clangConfig

    def __GetClangTidyPlatforms(self, clangTidyPlatforms: list[XmlClangTidyPlatform]) -> list[ClangTidyPlatform]:
        res: list[ClangTidyPlatform] = []
        for platform in clangTidyPlatforms:
            res.append(self.__GetClangTidyPlatform(platform))
        return res

    def __GetClangTidyPlatform(self, clangTidyPlatform: XmlClangTidyPlatform) -> ClangTidyPlatform:
        compiler = self.__GetClangTidyPlatformCompiler(clangTidyPlatform.Compiler)
        defines = self.__GetClangTidyPlatformDefines(clangTidyPlatform.Defines)
        strictChecks = self.__GetClangTidyPlatformStrictChecks(clangTidyPlatform.StrictChecks)
        return ClangTidyPlatform(clangTidyPlatform.Name, compiler, defines, strictChecks)

    def __GetClangTidyPlatformCompiler(self, compiler: XmlClangTidyPlatformCompiler | None) -> ClangTidyPlatformCompiler:
        if compiler is None:
            return ClangTidyPlatformCompiler([])
        return ClangTidyPlatformCompiler(compiler.Flags)

    def __GetClangTidyPlatformDefines(self, defines: XmlClangTidyPlatformDefines | None) -> ClangTidyPlatformDefines:
        definesAll: list[str] = []
        definesDebug: list[str] = []
        definesRelease: list[str] = []
        if defines is not None:
            definesAll += defines.All
            definesDebug += defines.Debug
            definesRelease += defines.Release
        return ClangTidyPlatformDefines(definesAll, definesDebug, definesRelease)

    def __GetClangTidyPlatformStrictChecks(self, strictChecks: XmlClangTidyPlatformStrictChecks | None) -> set[str]:
        if strictChecks is None:
            return set()
        return strictChecks.Checks

    def __GetCMakeConfiguration(self, configList: list[XmlCMakeConfiguration]) -> CMakeConfiguration:
        if len(configList) != 1:
            if len(configList) <= 0:
                return CMakeConfiguration("${TopProjectRoot}/build", None, CMakeUtil.GetMinimumVersion(), [], "Recipe.BuildTool.ninja")
            raise Exception("There can only be one CMakeConfiguration")
        configEntry = configList[0]

        defaultBuildDir = configEntry.DefaultBuildDir
        defaultInstallPrefix = configEntry.DefaultInstallPrefix
        minVersion = self.__ParseCMakeVersionString(configEntry.MinVersion)
        ninjaRecipePackageName = configEntry.NinjaRecipePackageName

        platformList: list[CMakeConfigurationPlatform] = []
        for platformEntry in configEntry.Platforms:
            # Default to the platform one if its defined, else default to the general one (which can be None)
            platformEntryDefaultInstallPrefix = platformEntry.DefaultInstallPrefix if platformEntry.DefaultInstallPrefix is not None else defaultInstallPrefix
            # Generate the platform config object
            platformList.append(
                CMakeConfigurationPlatform(
                    platformEntry.Name, platformEntry.DefaultGeneratorName, platformEntryDefaultInstallPrefix, platformEntry.AllowFindPackage
                )
            )

        if defaultBuildDir is None:
            raise Exception("CMakeConfiguration.DefaultBuildDir must be defined")
        return CMakeConfiguration(defaultBuildDir, defaultInstallPrefix, minVersion, platformList, ninjaRecipePackageName)

    def __ParseCMakeVersionString(self, versionStr: str | None) -> CMakeVersion:
        toolMin = CMakeUtil.GetMinimumVersion()
        if versionStr is None:
            return toolMin
        parsedMinVersion = Util.ParseVersionString(versionStr, maxValues=3)
        while len(parsedMinVersion) < 3:
            parsedMinVersion.append(0)
        projectMin = CMakeVersion(parsedMinVersion[0], parsedMinVersion[1], parsedMinVersion[2])
        return projectMin if projectMin >= toolMin else toolMin

    def __GenerateProjectInfo(
        self, log: Log, buildPlatformType: BuildPlatformType, projectRootConfig: XmlProjectRootConfigFile, noGitHash: bool
    ) -> ToolConfigProjectInfo:
        gitExeName = GitUtil.GetPlatformDependentExecutableName(buildPlatformType)
        rootProjectBasePackages = self.__ResolveBasePackages(log, projectRootConfig.XmlBasePackages, projectRootConfig.SourceFileName)
        gitHash = GitUtil.TryGetCurrentHash(gitExeName, projectRootConfig.RootDirectory) if not noGitHash else None
        result: list[ToolConfigProjectContext] = []
        projectVersion = Version.FromString(projectRootConfig.ProjectVersion)
        rootProjectContext = ToolConfigProjectContext(
            projectRootConfig.ProjectId, projectRootConfig.ProjectName, projectVersion, projectRootConfig.RootDirectory, gitHash, rootProjectBasePackages, None
        )
        result.append(rootProjectContext)
        topProjectContext = rootProjectContext
        # FIX: context base packages does not resolve correctly if we have multiple extension projects inheriting
        #      We basically need to do a proper graph to resolve the hierarchy (but for the current simple use cases of max one extend it works)
        if len(projectRootConfig.ExtendedProject) > 1:
            raise Exception("Only one extended project supported")
        for entry in projectRootConfig.ExtendedProject:
            contextBasePackages = self.__ResolveBasePackages(log, entry.XmlBasePackages, entry.SourceFileName)
            if len(rootProjectBasePackages) > 0:
                contextBasePackages = rootProjectBasePackages + contextBasePackages
            extendedProjectVersion = Version.FromString(entry.ProjectVersion)
            entryGitHash = GitUtil.TryGetCurrentHash(gitExeName, entry.RootDirectory) if not noGitHash else None
            extendedProjectContext = ToolConfigProjectContext(
                entry.ProjectId, entry.ProjectName, extendedProjectVersion, entry.RootDirectory, entryGitHash, contextBasePackages, rootProjectContext
            )
            result.append(extendedProjectContext)
            topProjectContext = extendedProjectContext
        return ToolConfigProjectInfo(result, topProjectContext)

    def __ResolveBasePackages(self, log: Log, basePackages: list[XmlConfigFileAddBasePackage], configFileName: str) -> list[ToolConfigBasePackage]:
        uniqueNameIds: set[str] = set()
        basePackageList: list[ToolConfigBasePackage] = []
        for basePackageEntry in basePackages:
            basePackage = ToolConfigBasePackage(log, basePackageEntry.Name)
            if basePackage.Id not in uniqueNameIds:
                uniqueNameIds.add(basePackage.Id)
                basePackageList.append(basePackage)
            else:
                raise DuplicatedConfigBasePackage(basePackage.Name, configFileName)
        basePackageList.sort(key=lambda s: s.Name)
        return basePackageList

    def GetVisualStudioDefaultVersion(self) -> int:
        visualStudioId = CompilerNames.VisualStudio.lower()
        if visualStudioId in self.CompilerConfigurationDict:
            return self.CompilerConfigurationDict[visualStudioId].DefaultVersion
        return VisualStudioVersion.DEFAULT

    def TryToPath(self, path: str | None) -> str | None:
        if path is None:
            return None
        return ToolConfigPackageRootUtil.TryToPath(self.RootDirectories, path)

    def ToPath(self, path: str) -> str:
        return ToolConfigPackageRootUtil.ToPath(self.RootDirectories, path)

    def TryFindRootDirectory(self, path: str | None) -> ToolConfigRootDirectory | None:
        return ToolConfigPackageRootUtil.TryFindRootDirectory(self.RootDirectories, path)

    def ToBashPath(self, path: str) -> str:
        if path.find("\\") >= 0:
            raise UsageErrorException(f"Backslash found in the supplied path '{path}'")
        for rootDir in self.RootDirectories:
            if path.startswith(rootDir.ResolvedPathEx):
                lenRootPath = len(rootDir.ResolvedPathEx)
                path = path[lenRootPath:]
                return rootDir.BashName + "/" + Util.UTF8ToAscii(path)
            elif path == rootDir.ResolvedPath:
                return rootDir.Name + "/"
        raise UsageErrorException(f"the folder '{path}' does not reside inside one of the root dirs")

    def TryLegacyToBashPath(self, path: str | None) -> str | None:
        if path is None:
            return None
        return self.ToBashPath(path)

    def ToBashPathDirectConversion(self, path: str) -> str:
        """This does not make the path relative to a root path"""
        if path.find("\\") >= 0:
            raise UsageErrorException(f"Backslash found in the supplied path '{path}'")
        path = Util.ChangeToBashEnvVariables(path)
        return Util.UTF8ToAscii(path).replace("\\", "/")

    def TryLegacyToBashPathDirectConversion(self, path: str | None) -> str | None:
        """This does not make the path relative to a root path"""
        if path is None:
            return None
        return self.ToBashPathDirectConversion(path)

    def ToDosPath(self, path: str) -> str:
        if path.find("\\") >= 0:
            raise UsageErrorException(f"Backslash found in the supplied path '{path}'")
        for rootDir in self.RootDirectories:
            if path.startswith(rootDir.ResolvedPathEx):
                lenRootPath = len(rootDir.ResolvedPathEx)
                path = path[lenRootPath:]
                tmp = rootDir.DosName + "/" + Util.UTF8ToAscii(path)
                return tmp.replace("/", "\\")
            elif path == rootDir.ResolvedPath:
                tmp = rootDir.Name + "/"
                return tmp.replace("/", "\\")
        raise UsageErrorException(f"the folder '{path}' does not reside inside one of the root dirs")

    def TryLegacyToDosPath(self, path: str | None) -> str | None:
        if path is None:
            return None
        return self.ToDosPath(path)

    def ToDosPathDirectConversion(self, path: str) -> str:
        """This does not make the path relative to a root path"""
        if path.find("\\") >= 0:
            raise UsageErrorException(f"Backslash found in the supplied path '{path}'")
        path = Util.ChangeToDosEnvironmentVariables(path)
        return Util.UTF8ToAscii(path).replace("/", "\\")

    def TryLegacyToDosPathDirectConversion(self, path: str | None) -> str | None:
        """This does not make the path relative to a root path"""
        if path is None:
            return None
        return self.ToDosPathDirectConversion(path)

    def ToCurrentOSPathDirectConversion(self, path: str) -> str:
        """Resolve the path to how it would look on the current OS"""
        return self.__ResolvedToCurrentOSPathDirectConversionMethod(path)

    def TryToCurrentOSPathDirectConversion(self, path: str | None) -> str | None:
        """Resolve the path to how it would look on the current OS"""
        return self.__ResolvedLegacyToCurrentOSPathDirectConversionMethod(path)

    def __ResolveNewProjectTemplateRootDirectories(
        self, basicConfig: BasicConfig, newProjectTemplateRootDirectories: list[XmlConfigFileAddNewProjectTemplatesRootDirectory]
    ) -> list[NewProjectTemplateRootDirectory]:
        uniqueIdDict: dict[str, NewProjectTemplateRootDirectory] = {}
        rootDirs: list[NewProjectTemplateRootDirectory] = []
        for rootDir in newProjectTemplateRootDirectories:
            toolRootDir = NewProjectTemplateRootDirectory(basicConfig, rootDir)
            if toolRootDir.Id not in uniqueIdDict:
                uniqueIdDict[toolRootDir.Id] = toolRootDir
                rootDirs.append(toolRootDir)
            else:
                raise DuplicatedNewProjectTemplatesRootPath(toolRootDir.Name, uniqueIdDict[toolRootDir.Id].Name, toolRootDir.Name)
        # We sort it so that the longest paths come first meaning we will always find the most exact match first
        # if searching from the front to the end of the list and comparing to 'startswith'
        rootDirs.sort(key=lambda s: -len(s.ResolvedPathEx))
        return rootDirs

    def __ResolveRootDirectories(
        self, basicConfig: BasicConfig, rootDirectories: list[XmlConfigFileAddRootDirectory], configFileName: str
    ) -> list[ToolConfigRootDirectory]:
        uniqueNames: set[str] = set()
        rootDirs: list[ToolConfigRootDirectory] = []
        for rootDir in rootDirectories:
            toolRootDir = ToolConfigRootDirectory(basicConfig, rootDir, rootDir.ProjectId)
            if toolRootDir.Name not in uniqueNames:
                uniqueNames.add(toolRootDir.Name)
                rootDirs.append(toolRootDir)
            else:
                raise DuplicatedConfigRootPath(toolRootDir.Name, configFileName)
        # We sort it so that the longest paths come first meaning we will always find the most exact match first
        # if searching from the front to the end of the list and comparing to 'startswith'
        rootDirs.sort(key=lambda s: -len(s.ResolvedPathEx))
        return rootDirs

    def __ResolveDirectories(self, basicConfig: BasicConfig, directories: list[XmlConfigFileAddTemplateImportDirectory]) -> list[ToolConfigDirectory]:
        dirs: list[ToolConfigDirectory] = []
        for dirEntry in directories:
            dirs.append(ToolConfigDirectory(basicConfig, dirEntry))
        return dirs

    def __ResolvePackageConfiguration(
        self,
        basicConfig: BasicConfig,
        rootDirs: list[ToolConfigRootDirectory],
        packageConfiguration: dict[str, XmlConfigPackageConfiguration],
        configFileName: str,
        projectRootDirectory: str,
    ) -> dict[str, ToolConfigPackageConfiguration]:
        configs = {}  # type Dict[str, ToolConfigPackageConfiguration]
        for packageConfig in list(packageConfiguration.values()):
            resolvedConfig = ToolConfigPackageConfiguration(basicConfig, rootDirs, packageConfig, configFileName, projectRootDirectory)
            configs[resolvedConfig.Name] = resolvedConfig
        return configs

    def __ResolveExperimental(
        self,
        basicConfig: BasicConfig,
        rootDirs: list[ToolConfigRootDirectory],
        experimental: XmlExperimental | None,
        configFileName: str,
        projectRootDirectory: str,
    ) -> ToolConfigExperimental | None:
        if experimental is None:
            return None
        return ToolConfigExperimental(basicConfig, rootDirs, experimental, configFileName, projectRootDirectory)

    def __TryResolveUnitTestPath(self) -> str | None:
        path = os.environ.get("FSL_GRAPHICS_INTERNAL")
        if path is None:
            return None
        return IOUtil.Join(path, "Tools/FslBuildGen/FslBuildGen/UnitTest/TestFiles")

    def __ProcessCompilerConfiguration(
        self, basicConfig: BasicConfig, xmlCompilerConfiguration: list[XmlConfigCompilerConfiguration]
    ) -> dict[str, ToolConfigCompilerConfiguration]:
        result: dict[str, ToolConfigCompilerConfiguration] = {}
        for config in xmlCompilerConfiguration:
            if config.Id in result:
                raise XmlDuplicatedCompilerConfigurationException(result[config.Id].BasedOn.XMLElement, result[config.Id].Name, config.XMLElement, config.Name)
            elif config.Name == CompilerNames.VisualStudio:
                result[config.Id] = ToolConfigCompilerConfiguration(basicConfig, config)
            else:
                msg = f"CompilerConfiguration name: '{config.Name}' is not currently supported, so entry is ignored"
                basicConfig.LogPrint(msg)
        return result
