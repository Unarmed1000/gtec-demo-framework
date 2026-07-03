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

import argparse
from typing import Any

from FslBuildGen import Main as MainFlow
from FslBuildGen import PackageListUtil, ParseUtil, PluginSharedValues
from FslBuildGen.Build import Builder
from FslBuildGen.Build.BuildVariantConfigUtil import BuildVariantConfigUtil
from FslBuildGen.Build.DataTypes import CommandType
from FslBuildGen.BuildExternal.RecipeInfo import RecipeInfo
from FslBuildGen.Config import Config
from FslBuildGen.Context.GeneratorContext import GeneratorContext
from FslBuildGen.DataTypes import PackageType

# from FslBuildGen.Generator import PluginConfig
from FslBuildGen.Engine.EngineResolveConfig import EngineResolveConfig
from FslBuildGen.Generator.GeneratorConfig import GeneratorConfig
from FslBuildGen.Generator.GeneratorDot import GeneratorDot
from FslBuildGen.Info import InfoSaver, PackageGraphQuery
from FslBuildGen.Info.PackageGraphQuery import PackageGraphIndex, QueryFormat

# from FslBuildGen.Log import Log
# from FslBuildGen.PackageConfig import PlatformNameString
# from FslBuildGen.PackageFilters import PackageFilters
from FslBuildGen.Packages.Package import Package
from FslBuildGen.Tool.AToolAppFlow import AToolAppFlow
from FslBuildGen.Tool.AToolAppFlowFactory import AToolAppFlowFactory
from FslBuildGen.Tool.ToolAppConfig import ToolAppConfig
from FslBuildGen.Tool.ToolAppContext import ToolAppContext
from FslBuildGen.Tool.ToolCommonArgConfig import ToolCommonArgConfig
from FslBuildGen.ToolConfig import ToolConfig
from FslBuildGen.VariableContextHelper import VariableContextHelper


class DefaultValue:
    PackageTypeList = "*"
    RequireFeaturesList = "*"
    UseExtensionsList = "*"
    UseFeaturesList = "*"
    # RequireExtensionsList = "*"

    # DryRun = False
    IgnoreNotSupported = False
    ListBuildVariants = False
    ListExtensions = False
    ListFeatures = False
    ListRecipes = False
    ListRequirements = False
    ListVariants = False
    Stats = False
    Details = False
    PackageConfigurationType = PluginSharedValues.TYPE_DEFAULT
    SaveJson: str | None = None
    IncludeGeneratorReport = False
    Graph = False
    DependsOn: str | None = None
    UsedBy: str | None = None
    DependencyPath: list[str] | None = None
    DumpGraph = False
    Transitive = False
    Format = QueryFormat.Text


class LocalToolConfig(ToolAppConfig):
    def __init__(self, packageTypeList: list[str]) -> None:
        super().__init__()

        self.PackageTypeList = packageTypeList

        # self.DryRun = DefaultValue.DryRun
        self.IgnoreNotSupported = DefaultValue.IgnoreNotSupported
        self.ListBuildVariants = DefaultValue.ListBuildVariants
        self.ListExtensions = DefaultValue.ListExtensions
        self.ListFeatures = DefaultValue.ListFeatures
        self.ListRecipes = DefaultValue.ListRecipes
        self.ListRequirements = DefaultValue.ListRequirements
        self.ListVariants = DefaultValue.ListVariants
        self.Stats = DefaultValue.Stats
        self.Details = DefaultValue.Details
        self.PackageConfigurationType = DefaultValue.PackageConfigurationType
        self.SaveJson = DefaultValue.SaveJson
        self.IncludeGeneratorReport = DefaultValue.IncludeGeneratorReport
        self.Graph = DefaultValue.Graph
        self.DependsOn = DefaultValue.DependsOn
        self.UsedBy = DefaultValue.UsedBy
        self.DependencyPath = DefaultValue.DependencyPath
        self.DumpGraph = DefaultValue.DumpGraph
        self.Transitive = DefaultValue.Transitive
        self.Format = DefaultValue.Format


def GetDefaultLocalConfig() -> LocalToolConfig:
    parsedPackageType = ParseUtil.ParsePackageTypeList(DefaultValue.PackageTypeList)
    return LocalToolConfig(parsedPackageType)


class ToolFlowBuildInfo(AToolAppFlow):
    # def __init__(self, toolAppContext: ToolAppContext) -> None:
    #    super().__init__(toolAppContext)

    def ProcessFromCommandLine(self, args: Any, currentDirPath: str, toolConfig: ToolConfig, userTag: object | None) -> None:
        # Process the input arguments here, before calling the real work function

        parsedPackageType = ParseUtil.ParsePackageTypeList(args.PackageType)

        localToolConfig = LocalToolConfig(parsedPackageType)

        # Configure the ToolAppConfig part
        localToolConfig.SetToolAppConfigValues(self.ToolAppContext.ToolAppConfig)

        # Configure the local part
        localToolConfig.IgnoreNotSupported = args.IgnoreNotSupported
        localToolConfig.ListBuildVariants = args.ListBuildVariants
        localToolConfig.ListExtensions = args.ListExtensions
        localToolConfig.ListFeatures = args.ListFeatures
        localToolConfig.ListRecipes = args.ListRecipes
        localToolConfig.ListRequirements = args.ListRequirements
        localToolConfig.ListVariants = args.ListVariants
        localToolConfig.Stats = args.stats
        localToolConfig.Details = args.details
        localToolConfig.PackageConfigurationType = args.type
        localToolConfig.SaveJson = args.SaveJson
        localToolConfig.IncludeGeneratorReport = args.IncludeGeneratorReport
        localToolConfig.Graph = args.graph
        localToolConfig.DependsOn = args.DependsOn
        localToolConfig.UsedBy = args.UsedBy
        localToolConfig.DependencyPath = args.DependencyPath
        localToolConfig.DumpGraph = args.DumpGraph
        localToolConfig.Transitive = args.Transitive
        localToolConfig.Format = args.Format

        self.Process(currentDirPath, toolConfig, localToolConfig)

    def Process(self, currentDirPath: str, toolConfig: ToolConfig, localToolConfig: LocalToolConfig) -> None:
        config = Config(
            self.Log, toolConfig, localToolConfig.PackageConfigurationType, localToolConfig.BuildVariantConstraints, localToolConfig.AllowDevelopmentPlugins
        )

        # if localToolConfig.DryRun:
        #    config.ForceDisableAllWrite()
        if localToolConfig.IgnoreNotSupported:
            config.IgnoreNotSupported = True

        # When emitting machine readable json for the graph queries we keep stdout to a single json document
        # (no title banner) so a consumer like a MCP server can parse it directly.
        if not self.__WantsCleanJsonOutput(localToolConfig):
            self.Log.PrintTitle()

        packageFilters = localToolConfig.BuildPackageFilters

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

        theFiles = MainFlow.DoGetFiles(
            config,
            toolConfig.GetMinimalConfig(generator.CMakeConfig),
            currentDirPath,
            localToolConfig.Recursive,
            additionalDirs=self.ToolAppContext.LowLevelToolConfig.AdditionalInputDirs,
        )
        generatorContext = GeneratorContext(
            config, self.ErrorHelpManager, packageFilters.RecipeFilterManager, config.ToolConfig.Experimental, generator, variableContext
        )
        # As we want to get info for all flavors we remove the limits
        packages = MainFlow.DoGetPackages(
            generatorContext, config, theFiles, packageFilters, autoAddRecipeExternals=False, engineResolveConfig=EngineResolveConfig.CreateNoLimit()
        )

        topLevelPackage = PackageListUtil.GetTopLevelPackage(packages)
        requestedFiles = None if config.IsSDKBuild else theFiles

        packageNameDetails = Builder.PackageNameDetails.ShowExactPackageName if localToolConfig.Details else Builder.PackageNameDetails.ShowTemplateName

        if localToolConfig.SaveJson is not None:
            if localToolConfig.BuildPackageFilters.ExtensionNameList is None:
                raise Exception("Invalid config missing ExtensionNameList filters")
            config.LogPrint(f"Saving to json file '{localToolConfig.SaveJson}'")

            generatorConfig = GeneratorConfig(generator.PlatformName, config.SDKConfigTemplatePath, config.ToolConfig, 1, CommandType.Build)
            InfoSaver.SavePackageMetaDataToJson(
                generatorContext,
                generatorConfig,
                localToolConfig.SaveJson,
                config,
                topLevelPackage,
                localToolConfig.PackageTypeList,
                localToolConfig.IncludeGeneratorReport,
            )

        if localToolConfig.ListFeatures:
            Builder.ShowFeatureList(self.Log, topLevelPackage, requestedFiles, packageNameDetails)
        if localToolConfig.ListVariants:
            requestedFiles = None if config.IsSDKBuild else theFiles
            Builder.ShowVariantList(self.Log, topLevelPackage, requestedFiles, generator)
        if localToolConfig.ListBuildVariants:
            Builder.ShowBuildVariantList(self.Log, generator)
        if localToolConfig.ListExtensions:
            Builder.ShowExtensionList(self.Log, topLevelPackage, requestedFiles, packageNameDetails)
        if localToolConfig.ListRequirements:
            Builder.ShowRequirementList(self.Log, topLevelPackage, requestedFiles, packageNameDetails)
        if localToolConfig.ListRecipes:
            RecipeInfo.ShowRecipeList(self.Log, topLevelPackage, requestedFiles)
        if localToolConfig.Stats:
            self.__ShowStats(topLevelPackage)

        if self.__HasGraphQuery(localToolConfig):
            self.__ProcessGraphQueries(topLevelPackage, localToolConfig)

        if localToolConfig.Graph:
            # Reuse the exact same dependency graph renderer that 'FslBuildGen --graph' uses.
            GeneratorDot(self.Log, config.ToolConfig, packages, generator.PlatformName)

    def __ShowStats(self, topLevelPackage: Package) -> None:
        exeCount = 0
        libCount = 0
        extLibCount = 0
        headerLibCount = 0
        toolRecipe = 0
        for dep in topLevelPackage.ResolvedAllDependencies:
            if dep.Package.Type == PackageType.Executable:
                exeCount += 1
            elif dep.Package.Type == PackageType.Library:
                libCount += 1
            elif dep.Package.Type == PackageType.ExternalLibrary:
                extLibCount += 1
            elif dep.Package.Type == PackageType.HeaderLibrary:
                headerLibCount += 1
            elif dep.Package.Type == PackageType.ToolRecipe:
                toolRecipe += 1

        self.Log.DoPrint(f"Total packages: {len(topLevelPackage.ResolvedAllDependencies)}")
        self.Log.DoPrint(f"- Exe:        {exeCount}")
        self.Log.DoPrint(f"- Lib:        {libCount}")
        self.Log.DoPrint(f"- ExtLib:     {extLibCount}")
        self.Log.DoPrint(f"- HeaderLib:  {headerLibCount}")
        self.Log.DoPrint(f"- ToolRecipe: {toolRecipe}")

    @staticmethod
    def __HasGraphQuery(localToolConfig: LocalToolConfig) -> bool:
        return (
            localToolConfig.DependsOn is not None
            or localToolConfig.UsedBy is not None
            or localToolConfig.DependencyPath is not None
            or localToolConfig.DumpGraph
        )

    @staticmethod
    def __WantsCleanJsonOutput(localToolConfig: LocalToolConfig) -> bool:
        return localToolConfig.Format == QueryFormat.Json and ToolFlowBuildInfo.__HasGraphQuery(localToolConfig)

    def __ProcessGraphQueries(self, topLevelPackage: Package, localToolConfig: LocalToolConfig) -> None:
        index = PackageGraphIndex(topLevelPackage)
        asJson = localToolConfig.Format == QueryFormat.Json
        transitive = localToolConfig.Transitive

        # In json mode we collect every requested query into a single document so a consumer (e.g. a MCP server)
        # can parse stdout directly without stripping anything.
        jsonResults: list[dict[str, Any]] = []

        if localToolConfig.DependsOn is not None:
            package = self.__ResolveQueryPackage(index, localToolConfig.DependsOn, asJson, jsonResults)
            if package is not None:
                edges = index.GetDependencies(package, transitive)
                if asJson:
                    jsonResults.append(PackageGraphQuery.BuildDependenciesResult(package, edges, transitive))
                else:
                    PackageGraphQuery.PrintDependencies(self.Log, package, edges, transitive)

        if localToolConfig.UsedBy is not None:
            package = self.__ResolveQueryPackage(index, localToolConfig.UsedBy, asJson, jsonResults)
            if package is not None:
                edges = index.GetDependents(package, transitive)
                if asJson:
                    jsonResults.append(PackageGraphQuery.BuildDependentsResult(package, edges, transitive))
                else:
                    PackageGraphQuery.PrintDependents(self.Log, package, edges, transitive)

        if localToolConfig.DependencyPath is not None:
            fromPackage = self.__ResolveQueryPackage(index, localToolConfig.DependencyPath[0], asJson, jsonResults)
            toPackage = self.__ResolveQueryPackage(index, localToolConfig.DependencyPath[1], asJson, jsonResults)
            if fromPackage is not None and toPackage is not None:
                paths = index.FindDependencyPaths(fromPackage, toPackage, None if asJson else self.Log)
                if asJson:
                    jsonResults.append(PackageGraphQuery.BuildDependencyPathsResult(fromPackage, toPackage, paths))
                else:
                    PackageGraphQuery.PrintDependencyPaths(self.Log, fromPackage, toPackage, paths)

        if localToolConfig.DumpGraph:
            adjacency = index.BuildAdjacency()
            if asJson:
                jsonResults.append(PackageGraphQuery.BuildGraphDumpResult(adjacency))
            else:
                PackageGraphQuery.PrintGraphDump(self.Log, adjacency)

        if asJson:
            self.Log.DoPrint(PackageGraphQuery.ResultsToJsonText(jsonResults))

    def __ResolveQueryPackage(self, index: PackageGraphIndex, name: str, asJson: bool, jsonResults: list[dict[str, Any]]) -> Package | None:
        package = index.TryResolve(name)
        if package is not None:
            return package
        suggestions = index.GetSuggestions(name)
        if asJson:
            # Keep errors inside the json document instead of writing plain text to stdout.
            jsonResults.append(PackageGraphQuery.BuildNotFoundResult(name, suggestions))
        else:
            message = f"Package '{name}' not found"
            if len(suggestions) > 0:
                message += ". Did you mean: {}".format(", ".join(suggestions))
            self.Log.DoPrint(message)
        return None


class ToolAppFlowFactory(AToolAppFlowFactory):
    # def __init__(self) -> None:
    #    pass

    def GetTitle(self) -> str:
        return "FslBuildInfo"

    def GetToolCommonArgConfig(self) -> ToolCommonArgConfig:
        argConfig = ToolCommonArgConfig()
        argConfig.AddPlatformArg = True
        argConfig.AddGeneratorSelection = True
        argConfig.ProcessRemainingArgs = False
        argConfig.SupportBuildTime = True
        argConfig.AddBuildFiltering = True
        argConfig.AddBuildThreads = True
        argConfig.AddBuildVariants = True
        argConfig.AllowRecursive = True
        return argConfig

    def AddCustomArguments(self, parser: argparse.ArgumentParser, toolConfig: ToolConfig, userTag: object | None) -> None:
        packageTypes = PackageType.AllStrings()
        packageTypes.sort()

        parser.add_argument("--graph", action="store_true", help="Generate a dependency graph image using dot (requires the graphviz dot executable in path)")
        parser.add_argument("--IgnoreNotSupported", action="store_true", help="try to build things that are marked as not supported")

        parser.add_argument("--ListBuildVariants", action="store_true", help="List all build-variants")
        parser.add_argument("--ListExtensions", action="store_true", help="List all extensions")
        parser.add_argument("--ListFeatures", action="store_true", help="List all features supported by build")
        parser.add_argument("--ListVariants", action="store_true", help="List all used variants")
        parser.add_argument("--ListRecipes", action="store_true", help="List all known recipes")
        parser.add_argument("--ListRequirements", action="store_true", help="List all requirements")

        parser.add_argument("--stats", action="store_true", help="Show stats")
        parser.add_argument("--details", action="store_true", help="Provide extended details")

        # Package dependency graph queries
        parser.add_argument("--DependsOn", default=DefaultValue.DependsOn, help="Show what the given package depends on")
        parser.add_argument("--UsedBy", default=DefaultValue.UsedBy, help="Show which packages depend on the given package")
        parser.add_argument(
            "--DependencyPath", default=DefaultValue.DependencyPath, nargs=2, metavar=("FROM", "TO"), help="Show the dependency path(s) from FROM to TO"
        )
        parser.add_argument("--DumpGraph", action="store_true", help="Dump the full package graph as an adjacency list")
        parser.add_argument(
            "--Transitive", action="store_true", help="Include the full transitive closure for --DependsOn and --UsedBy (default is direct only)"
        )
        parser.add_argument("--Format", default=DefaultValue.Format, choices=QueryFormat.AllStrings(), help="Output format for the dependency graph queries")

        parser.add_argument(
            "-t", "--type", default=DefaultValue.PackageConfigurationType, choices=[PluginSharedValues.TYPE_DEFAULT, "sdk"], help="Select generator type"
        )

        parser.add_argument("--SaveJson", default=DefaultValue.SaveJson, help="Save package information to the given json output file")
        parser.add_argument("--IncludeGeneratorReport", action="store_true", help="If set we include the generator report if available when saving to json")

        # filtering
        parser.add_argument(
            "--PackageType",
            default=DefaultValue.PackageTypeList,
            help="The list of package types that will be saved [{}]. For example [Executable] to save all packages of the 'Executable' type.".format(
                ", ".join(packageTypes)
            ),
        )

    def Create(self, toolAppContext: ToolAppContext) -> AToolAppFlow:
        return ToolFlowBuildInfo(toolAppContext)
