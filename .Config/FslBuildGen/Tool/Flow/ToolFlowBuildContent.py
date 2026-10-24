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
import os
from typing import Any

from FslBuildGen import IOUtil, PackageListUtil, ParseUtil, PluginSharedValues
from FslBuildGen import Main as MainFlow
from FslBuildGen.Build.BuildVariantConfigUtil import BuildVariantConfigUtil
from FslBuildGen.BuildConfig import Validate
from FslBuildGen.BuildContent import ContentDelivery
from FslBuildGen.BuildContent.SharedValues import CONFIG_FSLBUILDCONTENT_ENABLED
from FslBuildGen.BuildExternal import RecipeBuilder
from FslBuildGen.Config import Config
from FslBuildGen.Context.GeneratorContext import GeneratorContext
from FslBuildGen.DataTypes import PackageType

# from FslBuildGen.Generator import PluginConfig
# from FslBuildGen.Log import Log
# from FslBuildGen.PackageConfig import PlatformNameString
from FslBuildGen.Engine.EngineResolveConfig import EngineResolveConfig
from FslBuildGen.PackageFilters import PackageFilters
from FslBuildGen.Packages.Package import Package
from FslBuildGen.PlatformUtil import PlatformUtil
from FslBuildGen.Tool.AToolAppFlow import AToolAppFlow
from FslBuildGen.Tool.AToolAppFlowFactory import AToolAppFlowFactory
from FslBuildGen.Tool.ToolAppConfig import ToolAppConfig
from FslBuildGen.Tool.ToolAppContext import ToolAppContext
from FslBuildGen.Tool.ToolCommonArgConfig import ToolCommonArgConfig
from FslBuildGen.ToolConfig import ToolConfig
from FslBuildGen.ToolMinimalConfig import ToolMinimalConfig
from FslBuildGen.VariableContextHelper import VariableContextHelper


class DefaultValue:
    DependencyFile: str | None = None
    MonoGameContentProject = False
    Output: str | None = None
    PackageConfigurationType = PluginSharedValues.TYPE_DEFAULT
    Project: str | None = None
    Validate = False


class LocalToolConfig(ToolAppConfig):
    def __init__(self) -> None:
        super().__init__()
        self.DependencyFile = DefaultValue.DependencyFile
        self.MonoGameContentProject = DefaultValue.MonoGameContentProject
        self.Output = DefaultValue.Output
        self.PackageConfigurationType = DefaultValue.PackageConfigurationType
        self.Project = DefaultValue.Project
        self.Validate = DefaultValue.Validate


def GetDefaultLocalConfig() -> LocalToolConfig:
    return LocalToolConfig()


class ToolFlowBuildContent(AToolAppFlow):
    # def __init__(self, toolAppContext: ToolAppContext) -> None:
    #    super().__init__(toolAppContext)

    def ProcessFromCommandLine(self, args: Any, currentDirPath: str, toolConfig: ToolConfig, userTag: object | None) -> None:
        # Process the input arguments here, before calling the real work function
        localToolConfig = LocalToolConfig()

        # Configure the ToolAppConfig part
        localToolConfig.SetToolAppConfigValues(self.ToolAppContext.ToolAppConfig)

        # Configure the local part
        localToolConfig.DependencyFile = IOUtil.NormalizePath(args.DependencyFile) if args.DependencyFile is not None else None
        localToolConfig.MonoGameContentProject = args.MonoGameContentProject
        localToolConfig.Output = IOUtil.NormalizePath(args.output) if args.output is not None else None
        localToolConfig.PackageConfigurationType = args.type
        localToolConfig.Project = args.project
        localToolConfig.Validate = args.Validate

        self.Process(currentDirPath, toolConfig, localToolConfig)

    def Process(self, currentDirPath: str, toolConfig: ToolConfig, localToolConfig: LocalToolConfig) -> None:
        # Check if a environment variable has been set to disable this tool
        # This is for example done by FslBuild to prevent multiple executions of content building.
        toolEnabled = IOUtil.TryGetEnvironmentVariable(CONFIG_FSLBUILDCONTENT_ENABLED)

        featureList = localToolConfig.BuildPackageFilters.FeatureNameList

        config = Config(
            self.Log, toolConfig, localToolConfig.PackageConfigurationType, localToolConfig.BuildVariantConstraints, localToolConfig.AllowDevelopmentPlugins
        )
        config.DetectDuplicatePackages = localToolConfig.DetectDuplicatePackages

        # Get the platform and see if its supported
        buildVariantConfig = BuildVariantConfigUtil.GetBuildVariantConfig(localToolConfig.BuildVariantConstraints)
        variableContext = VariableContextHelper.Create(toolConfig, localToolConfig.UserSetVariables)
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
        generatorContext = GeneratorContext(
            config, self.ErrorHelpManager, localToolConfig.BuildPackageFilters.RecipeFilterManager, config.ToolConfig.Experimental, generator, variableContext
        )

        self.Log.LogPrint(f"Active platform: {generator.PlatformName}")

        discoverFeatureList = "*" in featureList
        topLevelPackage = None
        if discoverFeatureList or localToolConfig.Project is None:
            if discoverFeatureList:
                config.LogPrint("No features specified, so using package to determine them")
            topLevelPackage = self.__ResolveAndGetTopLevelPackage(
                generatorContext, config, currentDirPath, toolConfig.GetMinimalConfig(generator.CMakeConfig), localToolConfig.Recursive
            )
            if discoverFeatureList:
                featureList = [entry.Name for entry in topLevelPackage.ResolvedAllUsedFeatures]
            # if localToolConfig.Project is None:
            #    executeablePackage = PackageListUtil.FindFirstExecutablePackage(packages)
            #    localToolConfig.Project = executeablePackage.ShortName

        if localToolConfig.Validate:
            Validate.ValidatePlatform(config, localToolConfig.PlatformName, featureList)
            if topLevelPackage is None:
                topLevelPackage = self.__ResolveAndGetTopLevelPackage(
                    generatorContext, config, currentDirPath, toolConfig.GetMinimalConfig(generator.CMakeConfig), localToolConfig.Recursive
                )
            RecipeBuilder.ValidateInstallationForPackages(self.Log, config.SDKPath, generatorContext, topLevelPackage.ResolvedBuildOrder)

        if localToolConfig.MonoGameContentProject and localToolConfig.Recursive:
            raise Exception("--MonoGameContentProject is only allowed for single package builds")

        if toolEnabled is not None and not ParseUtil.ParseBool(toolEnabled):
            disabledMessage = f"FslBuildContent has been disabled by environment variable {CONFIG_FSLBUILDCONTENT_ENABLED} set to {toolEnabled}"
            if localToolConfig.Output is not None:
                # FslBuild only disables it for generators whose build files do not build the content, and those never ask for a output path
                raise Exception(f"{disabledMessage}, but the build expects content in '{localToolConfig.Output}'")
            self.Log.DoPrint(disabledMessage)
            if localToolConfig.MonoGameContentProject:
                # FslBuild delivered the content before it ran the build, the project the build reads is still made here
                if topLevelPackage is None:
                    topLevelPackage = self.__ResolveAndGetTopLevelPackage(
                        generatorContext, config, currentDirPath, toolConfig.GetMinimalConfig(generator.CMakeConfig), localToolConfig.Recursive
                    )
                package = self.__TryFindPackage(topLevelPackage, currentDirPath)
                if package is None:
                    raise Exception(f"Could not locate a package at '{currentDirPath}'")
                ContentDelivery.CheckMonoGameContentProject(package, generatorContext.GeneratorName, generator.DeliversAppContent, None)
                ContentDelivery.WriteMonoGameContentProject(self.Log, config.DisableWrite, package)
            return

        # The content of an app holds the content of the libraries it uses, so its packages are needed
        if topLevelPackage is None:
            topLevelPackage = self.__ResolveAndGetTopLevelPackage(
                generatorContext, config, currentDirPath, toolConfig.GetMinimalConfig(generator.CMakeConfig), localToolConfig.Recursive
            )

        if not localToolConfig.Recursive:
            package = self.__TryFindPackage(topLevelPackage, currentDirPath)
            if package is None:
                raise Exception(f"Could not locate a package at '{currentDirPath}'")
            ContentDelivery.DeliverPackage(
                self.Log,
                config.GetBuildDir(),
                config.DisableWrite,
                toolConfig,
                package,
                generatorContext.GeneratorName,
                generator.DeliversAppContent,
                localToolConfig.Output,
                localToolConfig.DependencyFile,
                localToolConfig.MonoGameContentProject,
            )
        else:
            if localToolConfig.DependencyFile is not None:
                raise Exception("--DependencyFile is only allowed for single package builds")
            if localToolConfig.Output is not None:
                raise Exception("--output is only allowed for single package builds")
            for foundPackage in topLevelPackage.ResolvedBuildOrder:
                if foundPackage.Type == PackageType.Executable:
                    ContentDelivery.DeliverPackage(
                        self.Log,
                        config.GetBuildDir(),
                        config.DisableWrite,
                        toolConfig,
                        foundPackage,
                        generatorContext.GeneratorName,
                        generator.DeliversAppContent,
                    )

    def __TryFindPackage(self, topLevelPackage: Package, packageDirPath: str) -> Package | None:
        packageId = os.path.normcase(os.path.normpath(packageDirPath))
        for package in topLevelPackage.ResolvedBuildOrder:
            # The same directory the way the file system compares names (on Windows 'e:/Work' is 'E:/work')
            if package.AbsolutePath is not None and os.path.normcase(os.path.normpath(package.AbsolutePath)) == packageId:
                return package
        return None

    def __ResolveAndGetTopLevelPackage(
        self, generatorContext: GeneratorContext, config: Config, currentDir: str, toolMiniConfig: ToolMinimalConfig, recursive: bool
    ) -> Package:
        # Since we use this to discover filters, we just use a empty one
        noPackageFilters = PackageFilters()

        theFiles = MainFlow.DoGetFiles(config, toolMiniConfig, currentDir, recursive, additionalDirs=self.ToolAppContext.LowLevelToolConfig.AdditionalInputDirs)
        packages = MainFlow.DoGetPackages(generatorContext, config, theFiles, noPackageFilters, engineResolveConfig=EngineResolveConfig.CreateDefaultFlavor())
        return PackageListUtil.GetTopLevelPackage(packages)


class ToolAppFlowFactory(AToolAppFlowFactory):
    # def __init__(self) -> None:
    #    pass

    def GetTitle(self) -> str:
        return "FslBuildContent"

    def GetToolCommonArgConfig(self) -> ToolCommonArgConfig:
        argConfig = ToolCommonArgConfig()
        argConfig.AddPlatformArg = True
        argConfig.AddGeneratorSelection = True
        argConfig.SupportBuildTime = True
        argConfig.AddUseFeatures = True
        argConfig.AddBuildFiltering = True
        argConfig.AddBuildThreads = True
        argConfig.AddBuildVariants = True
        argConfig.AllowRecursive = True
        argConfig.AllowDetectDuplicatePackages = True
        return argConfig

    def AddCustomArguments(self, parser: argparse.ArgumentParser, toolConfig: ToolConfig, userTag: object | None) -> None:
        parser.add_argument(
            "-t", "--type", default=DefaultValue.PackageConfigurationType, choices=[PluginSharedValues.TYPE_DEFAULT, "sdk"], help="Select generator type"
        )
        parser.add_argument("--project", default=DefaultValue.Project, help="The name of the project")
        parser.add_argument("--Validate", action="store_true", help="Do build config validation, like running FslBuildCheck")
        parser.add_argument(
            "--output",
            default=DefaultValue.Output,
            help='Set the build output directory, overriding the default "<package path>/Content" directory (experimental). Only allowed for single package builds',
        )
        parser.add_argument(
            "--DependencyFile",
            default=DefaultValue.DependencyFile,
            help="Write a make style dependency file that lists the content outputs and the files they depend on beyond Content.bld (like shader includes), for the build system. Only allowed for single package builds",
        )
        parser.add_argument(
            "--MonoGameContentProject",
            action="store_true",
            help="Also write the MonoGame content project of the app (Content.mgcb) into its resolved content directory, made from its content and the profiles of the files. For the project template of an app whose content MonoGame builds. Only allowed for single package builds",
        )

    def Create(self, toolAppContext: ToolAppContext) -> AToolAppFlow:
        return ToolFlowBuildContent(toolAppContext)
