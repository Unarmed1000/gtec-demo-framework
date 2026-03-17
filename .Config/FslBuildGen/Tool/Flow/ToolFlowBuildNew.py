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

import argparse
import difflib
import os

# import subprocess
import re
import shutil
from collections.abc import Iterable

# from typing import Callable
from typing import Any, cast

# from FslBuildGen.Generator import PluginConfig
# from FslBuildGen import PackageListUtil
# from FslBuildGen import ParseUtil
# from FslBuildGen import Util
# from FslBuildGen.Build import Builder
# from FslBuildGen.BasicConfig import BasicConfig
from FslBuildGen import IOUtil, PluginSharedValues, Util
from FslBuildGen import Main as MainFlow
from FslBuildGen.Build.BuildVariantConfigUtil import BuildVariantConfigUtil
from FslBuildGen.Config import BaseConfig, Config
from FslBuildGen.Context.GeneratorContext import GeneratorContext
from FslBuildGen.DataTypes import MagicStrings, PackageLanguage
from FslBuildGen.Engine.EngineResolveConfig import EngineResolveConfig
from FslBuildGen.Generator import GeneratorVCUtil
from FslBuildGen.PackageFilters import PackageFilters
from FslBuildGen.Packages.Package import Package
from FslBuildGen.PlatformUtil import PlatformUtil
from FslBuildGen.Template.TemplateFileProcessor import TemplateFileProcessor
from FslBuildGen.Template.TemplateFileRecordManager import TemplateFileRecordManager
from FslBuildGen.Tool.AToolAppFlow import AToolAppFlow
from FslBuildGen.Tool.AToolAppFlowFactory import AToolAppFlowFactory
from FslBuildGen.Tool.Flow import ToolFlowBuild
from FslBuildGen.Tool.ToolAppConfig import ToolAppConfig
from FslBuildGen.Tool.ToolAppContext import ToolAppContext
from FslBuildGen.Tool.ToolCommonArgConfig import ToolCommonArgConfig
from FslBuildGen.ToolConfig import NewProjectTemplateRootDirectory, ToolConfig, ToolConfigPackageConfiguration, ToolConfigPackageLocation
from FslBuildGen.ToolMinimalConfig import ToolMinimalConfig
from FslBuildGen.VariableContextHelper import VariableContextHelper
from FslBuildGen.Xml.XmlNewTemplateFile import XmlNewTemplateFile

g_templateFileName = "Template.xml"

g_templatePathCode = "Code"
g_templatePathFslGen = "FslGen"
g_projectInclude = "include"
g_projectSource = "source"

g_allowOverwriteOption = "--AllowOverwrite"


class GlobalStrings:
    SanityCheckProjectName = "sC_sYs"
    SanityCheckDir = "sC_tMp"


class UnknownTemplateException(Exception):
    def __init__(self, msg: str) -> None:
        # pylint: disable=useless-super-delegation
        super().__init__(msg)


class ArgumentError(Exception):
    """Indicate that a invalid argument was supplied"""


class FileConfig:
    def __init__(self, srcFile: str, dstFile: str) -> None:
        self.SrcFile = srcFile
        self.DstFile = dstFile


class ConfigCode:
    def __init__(self, templatePath: str, projectPath: str, projectName: str, prefixedProjectName: str) -> None:
        f1 = f"source/{projectName}_Register.cpp"
        f2 = f"source/{projectName}.hpp"
        f3 = f"source/{projectName}.cpp"

        self.Files: list[FileConfig] = []
        # quick fix to keep it compatible with the old tool
        self.__AddSpecialFile(self.Files, IOUtil.Join(templatePath, "source/DemoAppTemplate_Register.cpp"), IOUtil.Join(projectPath, f1))
        self.__AddSpecialFile(self.Files, IOUtil.Join(templatePath, "source/DemoAppTemplate.hpp"), IOUtil.Join(projectPath, f2))
        self.__AddSpecialFile(self.Files, IOUtil.Join(templatePath, "source/DemoAppTemplate.cpp"), IOUtil.Join(projectPath, f3))

        sourcePath = IOUtil.Join(templatePath, "source")
        projectSourcePath = IOUtil.Join(projectPath, "source")
        self.__AddFiles(self.Files, sourcePath, projectSourcePath)

    def __AddSpecialFile(self, files: list[FileConfig], srcPath: str, dstPath: str) -> None:
        if IOUtil.IsFile(srcPath):
            files.append(FileConfig(srcPath, dstPath))

    def __IsRegistered(self, fileConfigs: list[FileConfig], fileName: str) -> bool:
        return any(fileName == fileConfig.SrcFile for fileConfig in fileConfigs)

    def __AddFiles(self, fileConfigs: list[FileConfig], sourcePath: str, projectSourcePath: str) -> None:
        if not IOUtil.IsDirectory(sourcePath):
            return
        files = IOUtil.GetFilesAt(sourcePath, False)
        newFiles = []
        for file in files:
            if not self.__IsRegistered(fileConfigs, IOUtil.Join(sourcePath, file)):
                newFiles.append(file)
        for newFile in newFiles:
            self.Files.append(FileConfig(IOUtil.Join(sourcePath, newFile), IOUtil.Join(projectSourcePath, newFile)))


class ConfigFslGen:
    def __init__(self, genFileName: str, templatePath: str, projectPath: str, projectName: str, prefixedProjectName: str) -> None:
        f1 = genFileName
        self.Files = [FileConfig(os.path.join(templatePath, f1), os.path.join(projectPath, f1))]


class ConfigVariant:
    def __init__(self, genFileName: str, template: XmlNewTemplateFile, projectPath: str, projectName: str, packageName: str) -> None:
        self.Template = template
        self.ProjectName = projectName
        self.ProjectPath = projectPath
        self.PrefixedProjectName = template.Prefix + projectName

        self.PackageName = packageName
        self.PackageShortName, self.PackageNamespace = Util.GetPackageNames(packageName)
        self.PackageTargetName = packageName

        self.ProjectPrefix = template.Prefix
        self.ProjectPathInclude = os.path.join(projectPath, g_projectInclude)
        self.ProjectPathSource = os.path.join(projectPath, g_projectSource)
        self.TemplatePathCode = os.path.join(template.Path, g_templatePathCode)
        self.TemplatePathFslGen = os.path.join(template.Path, g_templatePathFslGen)
        self.ConfigCode = ConfigCode(self.TemplatePathCode, projectPath, projectName, self.PrefixedProjectName)
        self.ConfigFslGen = ConfigFslGen(genFileName, self.TemplatePathFslGen, projectPath, projectName, self.PrefixedProjectName)


def GetTemplatePaths(toolConfig: ToolConfig) -> list[NewProjectTemplateRootDirectory]:
    return toolConfig.NewProjectTemplateRootDirectories


class LocalConfig:
    def __init__(
        self,
        config: Config,
        projectPath: str,
        projectName: str,
        projectType: str,
        forced: bool,
        templateDict: dict[str, list[XmlNewTemplateFile]],
        reservedProjectNames: Iterable[str],
        strCurrentLanguage: str,
    ) -> None:
        self.CurrentYear = config.CurrentYearString
        self.ValidateProjectName(projectName)
        self.FslGraphicsSDKPath = config.SDKPath
        self.ProjectName = projectName
        self.ProjectPath = IOUtil.Join(projectPath, projectName)
        self.Template = self.__GetTemplate(templateDict, strCurrentLanguage, projectType.lower())
        self.TemplatePaths = GetTemplatePaths(config.ToolConfig)
        self.TemplatePathProjectType = self.Template.Path
        self.CurrentLanguageString = strCurrentLanguage

        resolvedPackageName = self.__DeterminePackageName(
            self.ProjectPath, projectName, config.ToolConfig.PackageConfiguration[MagicStrings.DefaultSDKConfiguration]
        )
        self.ValidatePackageName(resolvedPackageName, reservedProjectNames)
        self.PackageName = resolvedPackageName

        # Call this as the last thing since it references member variables
        templates: list[XmlNewTemplateFile] = templateDict.get(self.CurrentLanguageString, [])
        self.ConfigVariant = self.PrepareProjectType(config.GenFileName, templates, projectType, forced, self.PackageName)

    def __DeterminePackageName(self, fullProjectPath: str, projectName: str, packageConfiguration: ToolConfigPackageConfiguration) -> str:
        location = self.__FindLocation(packageConfiguration.Locations, fullProjectPath)

        fullPackageName = fullProjectPath[len(location.ResolvedPathEx) :].replace("/", ".")
        return fullPackageName

    def __FindLocation(self, locations: list[ToolConfigPackageLocation], fullProjectPath: str) -> ToolConfigPackageLocation:
        for location in locations:
            if fullProjectPath.startswith(location.ResolvedPathEx):
                return location
        raise Exception(f"Could not locate location for {fullProjectPath}")

    def __FindTemplateCandidatesThatAreClose(
        self, allTemplates: list[XmlNewTemplateFile], invalidTemplateName: str, ignoreList: list[str], matchRating: float = 0.70
    ) -> list[str]:
        """Find all candidates that have a close matchRating to the name we are looking for"""
        ratingList: list[tuple[float, str]] = []
        for template in allTemplates:
            name = template.Id
            if name not in ignoreList:
                ratio = difflib.SequenceMatcher(None, invalidTemplateName, name).ratio()
                if ratio > matchRating:
                    ratingList.append((ratio, template.Name))

        ratingList.sort(key=lambda s: -s[0])

        resultList: list[str] = []
        for entry in ratingList:
            resultList.append(entry[1])
        return resultList

    def __GetTemplate(self, templateDict: dict[str, list[XmlNewTemplateFile]], strCurrentLanguage: str, projectTypeId: str) -> XmlNewTemplateFile:
        templates = templateDict[strCurrentLanguage]
        for template in templates:
            if template.Id == projectTypeId:
                return template
        # Lets be nice and try to guess what the user could have meant
        candidateList = self.__FindTemplateCandidatesThatAreClose(templates, projectTypeId, [])
        if len(candidateList) <= 0:
            raise UnknownTemplateException(f"Unknown package template '{projectTypeId}'")
        raise UnknownTemplateException(f"Unknown package template '{projectTypeId}' did you mean {candidateList}")

    def ValidateProjectName(self, projectName: str) -> None:
        if len(projectName) < 1:
            raise ArgumentError("A project name needs to contain atleast one character")
        if re.match("[a-zA-Z0-9_]", projectName) is None:
            raise ArgumentError(f"A project name can only contain alpha numeric characters, digits and underscores '{projectName}'")
        if not projectName[0].isalpha():
            raise ArgumentError(f"A project name needs to start with a alpha character '{projectName}'")

    def ValidatePackageName(self, packageName: str, reservedProjectNames: Iterable[str]) -> None:
        if packageName in reservedProjectNames:
            raise ArgumentError(f"The given package name '{packageName}' is reserved")

    def PrepareProjectType(self, genFileName: str, templates: list[XmlNewTemplateFile], projectType: str, forced: bool, packageName: str) -> ConfigVariant:
        templateDict = {template.Id: template for template in templates}
        projectTypeId = projectType.lower()

        if projectTypeId in list(templateDict.keys()):
            template = templateDict[projectTypeId]
            if template.Template.Force:
                if len(template.Template.Warning) > 0:
                    print(f"WARNING: {template.Template.Warning}")
                if not forced:
                    raise ArgumentError("Use --Force to confirm you indeed wish to create such a project")
            return ConfigVariant(genFileName, template, self.ProjectPath, self.ProjectName, self.PackageName)
        else:
            templateNames = [template.Name for template in templates]
            raise ArgumentError("Unknown project type '{}'. Expected: {}.".format(projectType, ", ".join(templateNames)))


def ParsePackages(
    generatorContext: GeneratorContext, config: Config, toolMiniConfig: ToolMinimalConfig, currentDir: str, packageFilters: PackageFilters
) -> list[Package]:
    theFiles = MainFlow.DoGetFiles(config, toolMiniConfig, currentDir)
    # We set autoAddRecipeExternals to false since we are not actually interested in building this,
    # so therefore we dont want to do any checks for the externals before someone tries to build it
    return MainFlow.DoGetPackages(
        generatorContext, config, theFiles, packageFilters, autoAddRecipeExternals=False, engineResolveConfig=EngineResolveConfig.CreateDefaultFlavor()
    )


def GenerateProject(config: Config, localConfig: LocalConfig, configVariant: ConfigVariant, visualStudioGUID: str, genFileOnly: bool) -> None:
    #
    packageName = configVariant.PackageName
    packageShortName = configVariant.PackageShortName
    packageTargetName = configVariant.PackageTargetName
    packageCompany = config.ToolConfig.DefaultCompany
    strVariantList = config.VariantConstraints.AsString()

    templateFileRecordManager = TemplateFileRecordManager(localConfig.TemplatePathProjectType)
    templateFileProcessor = TemplateFileProcessor(config, "PlatformNotDefined", genFileOnly)
    templateFileProcessor.Environment.SetPackageValues(
        configVariant.ProjectPath,
        packageName,
        packageShortName,
        configVariant.ProjectPath,
        packageTargetName,
        packageTargetName,
        strVariantList,
        None,
        visualStudioGUID,
        config.CurrentYearString,
        packageCompany,
    )
    # templateFileProcessor.Environment.Set("##FEATURE_LIST##", featureList)
    templateFileProcessor.Process(config, templateFileRecordManager, configVariant.ProjectPath, None, None)

    # IOUtil.SafeMakeDirs(configVariant.ProjectPath)
    # IOUtil.SafeMakeDirs(configVariant.ProjectPathSource)
    # if not localConfig.ConfigVariant.Template.Template.NoInclude:
    #    IOUtil.SafeMakeDirs(configVariant.ProjectPathInclude)


def DetermineDirAndProjectName(currentDir: str, projectName: str) -> tuple[str, str]:
    if projectName == ".":
        projectName = IOUtil.GetFileName(currentDir)
        currentDir = IOUtil.GetDirectoryName(currentDir)
    return currentDir, projectName


class DefaultValue:
    AllowOverwrite = False
    # DryRun = False
    GenFileOnly = False
    Force = False
    Language = "NotDefined"
    NoBuildGen = False
    NoParse = False
    ProjectName = "NotDefined"
    SanityCheck = "off"
    Template = "NotDefined"
    VisualStudioGUID = GeneratorVCUtil.DefaultVCID
    ListTemplates = False


class LocalToolConfig(ToolAppConfig):
    def __init__(self) -> None:
        super().__init__()

        self.AllowOverwrite = DefaultValue.AllowOverwrite
        # self.DryRun = DefaultValue.DryRun
        self.GenFileOnly = DefaultValue.GenFileOnly
        self.Force = DefaultValue.Force
        self.Language = DefaultValue.Language
        self.NoBuildGen = DefaultValue.NoBuildGen
        self.NoParse = DefaultValue.NoParse
        self.ProjectName = DefaultValue.ProjectName
        self.SanityCheck = DefaultValue.SanityCheck
        self.Template = DefaultValue.Template
        self.VisualStudioGUID = DefaultValue.VisualStudioGUID
        self.ListTemplates = DefaultValue.ListTemplates


def GetDefaultLocalConfig(
    defaultPackageLanguage: str,
    template: str,
    projectName: str,
) -> LocalToolConfig:
    localToolConfig = LocalToolConfig()
    localToolConfig.Language = defaultPackageLanguage
    localToolConfig.ProjectName = projectName
    localToolConfig.Template = template
    return localToolConfig


class ToolFlowBuildNew(AToolAppFlow):
    # def __init__(self, toolAppContext: ToolAppContext) -> None:
    #    super().__init__(toolAppContext)

    def ProcessFromCommandLine(self, args: Any, currentDirPath: str, toolConfig: ToolConfig, userTag: object | None) -> None:
        # Process the input arguments here, before calling the real work function
        templateDict = cast(dict[str, list[XmlNewTemplateFile]], userTag)

        localToolConfig = LocalToolConfig()

        # Configure the ToolAppConfig part
        localToolConfig.SetToolAppConfigValues(self.ToolAppContext.ToolAppConfig)

        # Configure the local part
        localToolConfig.AllowOverwrite = args.AllowOverwrite
        localToolConfig.GenFileOnly = args.GenFileOnly
        localToolConfig.Force = args.Force
        localToolConfig.Language = args.Language
        localToolConfig.NoBuildGen = args.NoBuildGen
        localToolConfig.NoParse = args.NoParse
        localToolConfig.ProjectName = args.projectName
        localToolConfig.SanityCheck = args.SanityCheck
        localToolConfig.Template = args.template
        localToolConfig.VisualStudioGUID = args.i
        localToolConfig.ListTemplates = args.List

        self.Process(currentDirPath, toolConfig, localToolConfig, templateDict)

    def Process(self, currentDirPath: str, toolConfig: ToolConfig, localToolConfig: LocalToolConfig, templateDict: dict[str, list[XmlNewTemplateFile]]) -> None:
        if localToolConfig.ListTemplates:
            self.__ToolMainListTemplates(currentDirPath, toolConfig, localToolConfig, templateDict)
        elif localToolConfig.SanityCheck == "off" and localToolConfig.ProjectName != "*":
            self.__ToolMainEx(currentDirPath, toolConfig, localToolConfig, templateDict)
        else:
            self.__ToolMainSanityCheck(currentDirPath, toolConfig, localToolConfig, templateDict)

    def __BuildNow(self, config: Config, workDir: str, recursive: bool = False) -> None:
        toolFlowConfig = ToolFlowBuild.GetDefaultLocalConfig()
        toolFlowConfig.SetToolAppConfigValues(self.ToolAppContext.ToolAppConfig)
        toolFlowConfig.Recursive = recursive
        buildFlow = ToolFlowBuild.ToolFlowBuild(self.ToolAppContext)
        buildFlow.Process(workDir, config.ToolConfig, toolFlowConfig)

    def __PerformSanityCheck(self, basicConfig: Config, currentDir: str, projectName: str, template: XmlNewTemplateFile) -> None:
        projectDirName = IOUtil.Join(currentDir, projectName)
        self.__BuildNow(basicConfig, projectDirName)

    def __ToolMainListTemplates(
        self,
        currentDir: str,
        toolConfig: ToolConfig,
        localToolConfig: LocalToolConfig,
        templateDict: dict[str, list[XmlNewTemplateFile]],
        performSanityCheck: bool = False,
    ) -> None:
        config = Config(self.Log, toolConfig, "sdk", localToolConfig.BuildVariantConstraints, localToolConfig.AllowDevelopmentPlugins)
        config.PrintTitle()

        sortedLanguages = list(templateDict.keys())
        sortedLanguages.sort(key=lambda s: s.lower())

        for language in sortedLanguages:
            sortedTemplateEntries = list(templateDict[language])
            sortedTemplateEntries.sort(key=lambda s: s.Id.lower())
            print(f"Language: {language}")
            for templateEntry in sortedTemplateEntries:
                print(f"- {templateEntry.Name}")

    def __ToolMainEx(
        self,
        currentDir: str,
        toolConfig: ToolConfig,
        localToolConfig: LocalToolConfig,
        templateDict: dict[str, list[XmlNewTemplateFile]],
        performSanityCheck: bool = False,
    ) -> None:
        config = Config(self.Log, toolConfig, "sdk", localToolConfig.BuildVariantConstraints, localToolConfig.AllowDevelopmentPlugins)
        # config.ForceDisableAllWrite()

        config.PrintTitle()

        packageFilters = localToolConfig.BuildPackageFilters

        reservedProjectNames: set[str] = set()
        packages: list[Package] | None = None
        variableContext = VariableContextHelper.Create(toolConfig, localToolConfig.UserSetVariables)
        if not localToolConfig.NoParse:
            # Get the generator and see if its supported on this platform
            buildVariantConfig = BuildVariantConfigUtil.GetBuildVariantConfig(localToolConfig.BuildVariantConstraints)
            generator = self.ToolAppContext.PluginConfigContext.GetGeneratorPluginById(
                localToolConfig.PlatformName,
                localToolConfig.Generator,
                buildVariantConfig,
                variableContext.UserSetVariables,
                config.ToolConfig.DefaultPackageLanguage,
                config.ToolConfig.CMakeConfiguration,
                localToolConfig.GetUserCMakeConfig(),
                False,
            )
            PlatformUtil.CheckBuildPlatform(generator.PlatformName)
            config.LogPrint(f"Active platform: {generator.PlatformName}")
            generatorContext = GeneratorContext(
                config, self.ErrorHelpManager, packageFilters.RecipeFilterManager, config.ToolConfig.Experimental, generator, variableContext
            )
            packages = ParsePackages(generatorContext, config, toolConfig.GetMinimalConfig(generator.CMakeConfig), currentDir, packageFilters)

        # Reserve the name of all packages
        if packages is not None:
            for package in packages:
                reservedProjectNames.add(package.Name)

        currentDir, projectName = DetermineDirAndProjectName(currentDir, localToolConfig.ProjectName)
        localConfig = LocalConfig(
            config, currentDir, projectName, localToolConfig.Template, localToolConfig.Force, templateDict, reservedProjectNames, localToolConfig.Language
        )
        configVariant = localConfig.ConfigVariant

        if not localToolConfig.AllowOverwrite:
            if os.path.isdir(configVariant.ProjectPath):
                raise OSError(f"The project directory already exist: '{configVariant.ProjectPath}', you can use '{g_allowOverwriteOption}' to overwrite it.")
            elif os.path.exists(configVariant.ProjectPath):
                raise OSError(f"A file named '{configVariant.ProjectPath}' already exist, you can use '{g_allowOverwriteOption}' to overwrite it.")

        visualStudioGUID = localToolConfig.VisualStudioGUID
        if packages:
            visualStudioGUID = GeneratorVCUtil.GenerateGUID(config, packages, visualStudioGUID)

        GenerateProject(config, localConfig, configVariant, visualStudioGUID, localToolConfig.GenFileOnly)

        if not localToolConfig.NoBuildGen:
            config.DoPrint("Generating build files")
            projectConfig = Config(
                self.Log, toolConfig, PluginSharedValues.TYPE_DEFAULT, localToolConfig.BuildVariantConstraints, localToolConfig.AllowDevelopmentPlugins
            )

            theFiles = MainFlow.DoGetFiles(projectConfig, toolConfig.GetMinimalConfig(generator.CMakeConfig), configVariant.ProjectPath)
            buildVariantConfig = BuildVariantConfigUtil.GetBuildVariantConfig(localToolConfig.BuildVariantConstraints)
            platformGeneratorPlugin = self.ToolAppContext.PluginConfigContext.GetGeneratorPluginById(
                localToolConfig.PlatformName,
                localToolConfig.Generator,
                buildVariantConfig,
                variableContext.UserSetVariables,
                config.ToolConfig.DefaultPackageLanguage,
                config.ToolConfig.CMakeConfiguration,
                localToolConfig.GetUserCMakeConfig(),
                False,
            )
            MainFlow.DoGenerateBuildFiles(
                self.ToolAppContext.PluginConfigContext,
                projectConfig,
                variableContext,
                self.ErrorHelpManager,
                theFiles,
                platformGeneratorPlugin,
                packageFilters,
            )

            if performSanityCheck:
                self.__PerformSanityCheck(config, currentDir, localConfig.ProjectName, localConfig.Template)

    def __RunToolMainForSanityCheck(
        self,
        currentDir: str,
        toolConfig: ToolConfig,
        localToolConfig: LocalToolConfig,
        templateDict: dict[str, list[XmlNewTemplateFile]],
        debugMode: bool,
        templateList: list[str],
    ) -> None:
        currentDir = IOUtil.Join(currentDir, GlobalStrings.SanityCheckDir)
        IOUtil.SafeMakeDirs(currentDir)
        if not IOUtil.IsDirectory(currentDir):
            raise Exception(f"could not create work directory: '{currentDir}'")

        isBuilding = False
        try:
            for currentTemplateName in templateList:
                if currentTemplateName == "*" or currentTemplateName.startswith("/") or ".." in currentTemplateName:
                    raise Exception("Usage error")

                localToolConfig.Template = currentTemplateName

                localToolConfig.ProjectName = f"{GlobalStrings.SanityCheckProjectName}_{localToolConfig.Template}"
                localToolConfig.Force = True

                if debugMode:
                    generatedDir = IOUtil.Join(currentDir, localToolConfig.ProjectName)
                    if IOUtil.IsDirectory(generatedDir):
                        continue

                print(f"Generating sanity project for template '{localToolConfig.Template}' begin")
                self.__ToolMainEx(currentDir, toolConfig, localToolConfig, templateDict, False)
                print(f"Generating sanity project for template '{localToolConfig.Template}' ended successfully")

            isBuilding = True
            config = Config(self.Log, toolConfig, "sdk", localToolConfig.BuildVariantConstraints, localToolConfig.AllowDevelopmentPlugins)
            print(f"Building sanity projects for all template begin {localToolConfig.Template}")
            self.__BuildNow(config, currentDir, True)
            print(f"Building sanity project for template end {localToolConfig.Template}")
        except:
            if not isBuilding:
                print(f"Sanity check of template '{localToolConfig.Template}' failed")
            else:
                print("Sanity build of templates failed")
            raise
        finally:
            if not debugMode:
                for currentTemplateName in templateList:
                    if currentTemplateName == "*" or currentTemplateName.startswith("/") or ".." in currentTemplateName:
                        raise Exception("Usage error")
                    projectName = f"{GlobalStrings.SanityCheckProjectName}_{currentTemplateName}"

                    projectDir = IOUtil.Join(currentDir, projectName)
                    if IOUtil.IsDirectory(projectDir):
                        shutil.rmtree(projectDir)

            # try:
            #    if debugMode:
            #        generatedDir = IOUtil.Join(currentDir, localToolConfig.ProjectName)
            #        if IOUtil.IsDirectory(generatedDir):
            #            return

            #    print(("Sanity check of template '{0}' begin".format(localToolConfig.Template)))
            #    self.__ToolMainEx(currentDir, toolConfig, localToolConfig, templateDict, True)
            #    print(("Sanity check of template '{0}' ended successfully".format(localToolConfig.Template)))
            # except:
            #    print(("Sanity check of template '{0}' failed".format(localToolConfig.Template)))
            #    raise
            # finally:
            #    if not debugMode:
            #        projectDir = IOUtil.Join(currentDir, localToolConfig.ProjectName)
            #        if IOUtil.IsDirectory(projectDir):
            #            shutil.rmtree(projectDir)

    def __ToolMainSanityCheck(
        self,
        currentDir: str,
        toolConfig: ToolConfig,
        localToolConfig: LocalToolConfig,
        templateDict: dict[str, list[XmlNewTemplateFile]],
    ) -> None:
        if localToolConfig.SanityCheck == "off":
            raise Exception("SanityCheck not enabled but projectName was set to '*'")
        if localToolConfig.ProjectName != "*":
            raise Exception("SanityCheck enabled but projectName was not set to '*'")
        if localToolConfig.SanityCheck != "on" and localToolConfig.SanityCheck != "debug":
            raise Exception(f"SanityCheck '{localToolConfig.SanityCheck}' is not valid, expected 'on' or 'debug'")

        if len(localToolConfig.Template) <= 0:
            raise UnknownTemplateException("Template can not be a empty string")

        debugMode = localToolConfig.SanityCheck == "debug"

        if localToolConfig.Template != "*":
            self.__RunToolMainForSanityCheck(currentDir, toolConfig, localToolConfig, templateDict, debugMode, [localToolConfig.Template])
        else:
            sortedLanguages = list(templateDict.keys())
            sortedLanguages.sort(key=lambda s: s.lower())

            for language in sortedLanguages:
                sortedTemplateEntries = list(templateDict[language])
                sortedTemplateEntries.sort(key=lambda s: s.Id.lower())
                allTemplates = [templateEntry.Name for templateEntry in sortedTemplateEntries]
                self.__RunToolMainForSanityCheck(currentDir, toolConfig, localToolConfig, templateDict, debugMode, allTemplates)


def TryFind(templates: list[XmlNewTemplateFile], newEntry: XmlNewTemplateFile) -> XmlNewTemplateFile | None:
    for entry in templates:
        if entry.Id == newEntry.Id:
            return entry
    return None


class ToolAppFlowFactory(AToolAppFlowFactory):
    # def __init__(self) -> None:
    #    pass

    def GetTitle(self) -> str:
        return "FslBuildNew"

    def GetShortDesc(self) -> str | None:
        return "SDK New project wizard"

    def GetToolCommonArgConfig(self) -> ToolCommonArgConfig:
        argConfig = ToolCommonArgConfig()
        argConfig.AddPlatformArg = True
        # argConfig.AllowVSVersion = True
        argConfig.SupportBuildTime = True
        argConfig.AddBuildFiltering = True
        argConfig.AddBuildThreads = True
        argConfig.AddBuildVariants = True
        return argConfig

    def CreateUserTag(self, baseConfig: BaseConfig) -> object | None:
        templateRootPaths = GetTemplatePaths(baseConfig.ToolConfig)
        subDirs: list[str] = []
        for entry in templateRootPaths:
            subDirs += IOUtil.GetDirectoriesAt(entry.ResolvedPath, True)

        templates: dict[str, list[XmlNewTemplateFile]] = {}
        for currentDir in subDirs:
            languageDir = IOUtil.GetFileName(currentDir)
            dirs = IOUtil.GetDirectoriesAt(currentDir, True)
            for possibleDir in dirs:
                templatePath = IOUtil.Join(possibleDir, g_templateFileName)
                if IOUtil.IsFile(templatePath):
                    if languageDir not in templates:
                        templates[languageDir] = []
                    xmlNewTemplateFile = XmlNewTemplateFile(baseConfig, templatePath)
                    existingTemplateFile = TryFind(templates[languageDir], xmlNewTemplateFile)
                    if existingTemplateFile is None:
                        templates[languageDir].append(xmlNewTemplateFile)
                    else:
                        raise Exception(
                            f"Duplicated template name '{xmlNewTemplateFile.Name}' found at '{xmlNewTemplateFile.Path}' and '{existingTemplateFile.Path}'"
                        )

        # sort the templates
        for listEntry in templates.values():
            listEntry.sort(key=lambda s: s.Name.lower())

        removeKeys = [key for key in templates if len(templates[key]) <= 0]
        for key in removeKeys:
            templates.pop(key)
        return templates

    def AddCustomArguments(self, parser: argparse.ArgumentParser, toolConfig: ToolConfig, userTag: object | None) -> None:
        templateDict = cast(dict[str, list[XmlNewTemplateFile]], userTag)
        templateList = []

        sortedLanguages = list(templateDict.keys())
        sortedLanguages.sort(key=lambda s: s.lower())

        formatString = "{0} [{1}]" if len(templateDict) > 1 else "{1}"
        for language in sortedLanguages:
            languageTemplateList = templateDict[language]
            languageTemplateNames = [entry.Name for entry in languageTemplateList]
            languageTemplates = ", ".join(languageTemplateNames)
            templateList.append(formatString.format(language, languageTemplates))

        templates = ", ".join(templateList)
        defaultLanguage = PackageLanguage.ToString(toolConfig.DefaultPackageLanguage)

        parser.add_argument("template", help=f"The template for the new project: {templates}")
        parser.add_argument("projectName", help="The name of the new project (if you want to use the existing directory then specify '.' here")
        parser.add_argument("--NoParse", action="store_true", help="Disable the package parsing and validation")
        parser.add_argument("--NoBuildGen", action="store_true", help="Disable platform build file generation")
        parser.add_argument("--Force", action="store_true", help="Force the project type to be created")
        parser.add_argument("-i", default=DefaultValue.VisualStudioGUID, help="Visual studio project id")
        parser.add_argument(
            "-l",
            "--Language",
            default=defaultLanguage,
            help="Select the language for the template [{}]. Defaults to: {}".format(", ".join(sortedLanguages), defaultLanguage),
        )
        parser.add_argument(
            g_allowOverwriteOption, action="store_true", help="Allow existing files to be overwritten (for example if a directory already exist)"
        )
        parser.add_argument("--GenFileOnly", action="store_true", help=f"Only write the gen file '{toolConfig.GenFileName}' file")
        parser.add_argument(
            "--SanityCheck",
            default=DefaultValue.SanityCheck,
            help="off = disabled, on=enabled, debug=enabled, leave files behind. Combine this with a project name of '*' to start a template sanity check. If the template is set to '*' all templates are sanity checked",
        )
        parser.add_argument("--List", action="store_true", help="List all available templates and exit (this ignores the template and projectName)")

    def Create(self, toolAppContext: ToolAppContext) -> AToolAppFlow:
        return ToolFlowBuildNew(toolAppContext)
