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

# from typing import Tuple
import argparse

# import os
import sys
import time
from datetime import timedelta

# from typing import Callable
from typing import Any

from FslBuildGen import IOUtil, ParseUtil

# from FslBuildGen import PluginSharedValues
# from FslBuildGen import Util
from FslBuildGen.BasicConfig import BasicConfig
from FslBuildGen.BuildConfig.BuildVariables import BuildVariables
from FslBuildGen.BuildConfig.UserSetVariables import UserSetVariables
from FslBuildGen.Config import BaseConfig
from FslBuildGen.DataTypes import BuildThreads, GeneratorNameString, GeneratorType

# from FslBuildGen.DataTypes import VisualStudioVersion
from FslBuildGen.ErrorHelpManager import ErrorHelpManager
from FslBuildGen.Exceptions import AggregateException, ExitException, GroupedException
from FslBuildGen.Generator import PluginConfig
from FslBuildGen.Generator.PluginConfigContext import PluginConfigContext
from FslBuildGen.Log import Log
from FslBuildGen.PlatformUtil import PlatformUtil
from FslBuildGen.Tool.AToolAppFlowFactory import AToolAppFlowFactory
from FslBuildGen.Tool.LowLevelToolConfig import LowLevelToolConfig
from FslBuildGen.Tool.ToolAppConfig import DefaultValue, ToolAppConfig
from FslBuildGen.Tool.ToolAppContext import ToolAppContext
from FslBuildGen.Tool.ToolCommonArgConfig import ToolCommonArgConfig
from FslBuildGen.ToolConfig import ToolConfig
from FslBuildGen.Version import Version
from FslBuildGen.Xml.Project.XmlProjectRootConfigFile import XmlProjectRootConfigFile

CurrentVersion = Version(3, 10, 0, 0)


def __AddDefaultOptions(parser: argparse.ArgumentParser, allowStandaloneMode: bool) -> None:
    parser.add_argument("-v", "--verbosity", action="count", default=0, help="Set verbosity level")
    parser.add_argument("--scriptDebug", action="store_true", help="Enable script debugging")
    parser.add_argument("--dev", action="store_true", help="Allow plugins in development")
    parser.add_argument("--version", action="store_true", help="The tool version")
    parser.add_argument("--profile", action="store_true", help="Enable tool profiling")
    parser.add_argument(
        "--input", action="append", default=None, help="Set the input directory (defaults to current working directory). Can be specified multiple times."
    )
    parser.add_argument("--inputFile", default=None, help="A file containing input directories, one per line.")
    parser.add_argument("--noGitHash", action="store_true", help="Disable getting the current git hash (useful for CI builds).")
    # parser.add_argument('--buildDir', default=None, help='Set the build directory (experimental only supported with cmake)')
    if allowStandaloneMode:
        parser.add_argument("--standalone", action="store_true", help="Allow the tool to run without the SDK present. This is a experimental option.")


def __ResolveAllInputDirs(inputArgs: list[str] | None, inputFile: str | None, verbosityLevel: int) -> list[str]:
    """Resolve all input directories from --input args and --inputFile, returning a deduplicated list.
    The first entry is the primary dir (or cwd if none specified)."""
    rawDirs: list[str] = []
    if inputArgs is not None:
        rawDirs.extend(inputArgs)
    if inputFile is not None:
        with open(inputFile, encoding="utf-8-sig") as f:
            for line in f:
                stripped = line.strip()
                if len(stripped) > 0 and not stripped.startswith("#"):
                    rawDirs.append(stripped)
    if len(rawDirs) == 0:
        return [IOUtil.GetCurrentWorkingDirectory()]
    seen: set[str] = set()
    result: list[str] = []
    for rawDir in rawDirs:
        normalizedDir = IOUtil.NormalizePath(rawDir)
        if not IOUtil.IsDirectory(normalizedDir):
            raise Exception(f"Path '{normalizedDir}' specified by --input is not a directory")
        if normalizedDir not in seen:
            seen.add(normalizedDir)
            result.append(normalizedDir)
    if verbosityLevel > 4:
        for entry in result:
            print(f"Using input directory '{entry}'")
    return result


def __EarlyArgumentParser(allowStandaloneMode: bool) -> LowLevelToolConfig | None:
    ### Parse the initial options this allows us to use the required debug and verbosity levels while
    ### creating the actual command line argumnets.
    debugEnabled = False
    try:
        parser = argparse.ArgumentParser(add_help=False)
        __AddDefaultOptions(parser, allowStandaloneMode)
        args, unknown = parser.parse_known_args()
        verbosityLevel = args.verbosity
        debugEnabled = bool(args.scriptDebug)
        allowDevelopmentPlugins = bool(args.dev)
        profilerEnabled = bool(args.profile)
        noGitHash = bool(args.noGitHash)
        standaloneEnabled = False if not allowStandaloneMode else (bool(args.standalone))
        allInputDirs = __ResolveAllInputDirs(args.input, args.inputFile, verbosityLevel)
        currentDir = allInputDirs[0]
        additionalInputDirs = allInputDirs[1:]

        if args.version:
            print(f"V{CurrentVersion} unofficial")

        return LowLevelToolConfig(
            verbosityLevel, debugEnabled, allowDevelopmentPlugins, profilerEnabled, standaloneEnabled, currentDir, additionalInputDirs, noGitHash
        )
    except Exception as ex:
        print(f"ERROR: {str(ex)}")
        if not debugEnabled:
            return None
        raise


def __ProcessRemainingArgs(args: list[str] | None) -> list[str]:
    if args is None or len(args) <= 0:
        return []

    if args[0] != "--":
        raise Exception(f"Expected '--' before additional build options not '{args[0]}'")

    return args[1:]


class BuildTimer:
    def __init__(self) -> None:
        super().__init__()
        self.StartTime = time.time()

    def GetTimePassed(self) -> float:
        return time.time() - self.StartTime


def PrintBuildTiming(timing: BuildTimer) -> None:
    timeString = str(timedelta(seconds=timing.GetTimePassed()))
    print(f"Build time: {timeString}")


def __GetProjectRootConfigPath(localConfig: LowLevelToolConfig, currentDir: str, projectRootFileName: str) -> str:
    try:
        res = IOUtil.TryFindFileInCurrentOrParentDir(currentDir, projectRootFileName)
        if res is not None:
            return res
    except Exception as ex:
        if localConfig.VerbosityLevel > 0:
            print(f"ERROR: {ex}")
        if localConfig.DebugEnabled:
            raise
    raise Exception(f"Project configuration file '{projectRootFileName}' could not be located in current or parent folder")


def GetProjectRootConfig(localConfig: LowLevelToolConfig, basicConfig: BasicConfig, currentDir: str) -> XmlProjectRootConfigFile:
    """Try to locate a instance of the 'Project.gen' file in the current or parent directories"""
    path = __GetProjectRootConfigPath(localConfig, currentDir, "Project.gen")
    return XmlProjectRootConfigFile(basicConfig, path)


def __GetToolConfigPath(defaultPath: str) -> str:
    """Retrieve the full path of the tool config file"""
    toolConfigPath = defaultPath
    if not toolConfigPath:
        toolConfigPath = IOUtil.GetExecutablePath()
        toolConfigPath = IOUtil.Join(toolConfigPath, "FslBuildGen.xml")
    return toolConfigPath


def __CreateToolAppConfig(
    args: Any, defaultPlatform: str, toolCommonArgConfig: ToolCommonArgConfig, defaultVSVersion: int, userSetVariables: UserSetVariables
) -> ToolAppConfig:
    # Configure the ToolAppConfig part
    toolAppConfig = ToolAppConfig()
    toolAppConfig.DefaultPlatformName = defaultPlatform
    toolAppConfig.UserSetVariables = userSetVariables

    toolAppConfig.AllowDevelopmentPlugins = bool(args.dev)
    if toolCommonArgConfig.AddPlatformArg:
        toolAppConfig.PlatformName = args.platform
    else:
        toolAppConfig.PlatformName = defaultPlatform

    if toolCommonArgConfig.ProcessRemainingArgs:
        toolAppConfig.RemainingArgs = args.RemainingArgs
    if toolCommonArgConfig.AllowForceClaimInstallArea:
        toolAppConfig.ForceClaimInstallArea = args.ForceClaimInstallArea

    toolAppConfig.VSVersion = int(args.VSVersion) if hasattr(args, "VSVersion") else defaultVSVersion

    if toolCommonArgConfig.AddBuildFiltering or toolCommonArgConfig.AddUseFeatures:
        toolAppConfig.BuildPackageFilters.FeatureNameList = ParseUtil.ParseFeatureList(args.UseFeatures)

    if toolCommonArgConfig.AddBuildFiltering:
        toolAppConfig.BuildPackageFilters.RequiredFeatureNameList = ParseUtil.ParseFeatureList(args.RequireFeatures)
        toolAppConfig.BuildPackageFilters.ExtensionNameList = ParseUtil.ParseExtensionList(args.UseExtensions)
        toolAppConfig.BuildPackageFilters.RecipeFilterManager = ParseUtil.ParseRecipeList(args.Recipes)

    if toolCommonArgConfig.AddBuildFilteringExePackageName:
        toolAppConfig.BuildPackageFilters.ExePackageNameFilter = args.ExePackageNameFilter

    if toolCommonArgConfig.AddBuildVariants:
        forceDebugBuild = False if args.debug is None else args.debug
        toolAppConfig.BuildVariantConstraints = ParseUtil.ParseExternalVariantConstraints(args.Variants, forceDebugBuild)

    if toolCommonArgConfig.AddBuildThreads:
        toolAppConfig.BuildThreads = BuildThreads.FromString(args.BuildThreads)

    if toolCommonArgConfig.AllowRecursive:
        toolAppConfig.Recursive = args.recursive

    if hasattr(args, "Generator"):
        # Convert to the internal value instead of string
        toolAppConfig.Generator = GeneratorType.FromString(args.Generator)

    # Handle the CMake parameters
    if hasattr(args, "CMakeBuildDir"):
        toolAppConfig.CMakeBuildDir = None if args.CMakeBuildDir is None else IOUtil.NormalizePath(args.CMakeBuildDir)
    if hasattr(args, "CMakeBuildDirId"):
        toolAppConfig.CMakeBuildDirId = None if args.CMakeBuildDirId is None else int(args.CMakeBuildDirId)
    if hasattr(args, "CMakeInstallPrefix"):
        toolAppConfig.CMakeInstallPrefix = None if args.CMakeInstallPrefix is None else IOUtil.NormalizePath(args.CMakeInstallPrefix)
    if hasattr(args, "CMakeGeneratorName"):
        toolAppConfig.CMakeGeneratorName = args.CMakeGeneratorName
    if hasattr(args, "CMakeConfigArgs"):
        toolAppConfig.CMakeConfigArgs = args.CMakeConfigArgs
    if hasattr(args, "CMakeConfigGlobalArgs"):
        toolAppConfig.CMakeConfigGlobalArgs = args.CMakeConfigGlobalArgs
    if hasattr(args, "CMakeAllowFindPackage"):
        toolAppConfig.CMakeAllowFindPackage = None if args.CMakeAllowFindPackage is None else ParseUtil.ParseBool(args.CMakeAllowFindPackage)

    return toolAppConfig


def __PrepareGeneratorPlugins(
    pluginConfigContext: PluginConfigContext, lowLevelToolConfig: LowLevelToolConfig, toolCommonArgConfig: ToolCommonArgConfig
) -> list[str]:
    generatorPlugins = pluginConfigContext.GetGeneratorPlugins()
    generatorIds = [entry.PlatformId for entry in generatorPlugins]
    return generatorIds


def __ParseUserSetVariables(log: Log, setArguments: list[str] | None) -> UserSetVariables:
    variableDict: dict[str, str] = {}
    if setArguments is not None and len(setArguments) > 0:
        setArguments.sort()
        sortedList = BuildVariables.GetSetableVariables()
        sortedList.sort()
        validArguments = set(sortedList)
        for entry in setArguments:
            res = entry.split("=")
            if len(res) != 2:
                raise Exception(f"the set variable '{entry}' was not in the expected 'name=value' format")
            key = res[0].strip()
            value = res[1].strip()
            if key not in validArguments:
                raise Exception(f"the set variable '{key}' is unknown. Valid variables are {sortedList}")
            if len(value) <= 0:
                raise Exception(f"the set variable '{key}' must have a valid value.")
            variableDict[key] = value
            log.LogPrintVerbose(2, f"user set '{key}={value}'")
    return UserSetVariables(variableDict)


def __CreateParser(toolCommonArgConfig: ToolCommonArgConfig, allowStandaloneMode: bool) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="FslBuildTools")
    __AddDefaultOptions(parser, allowStandaloneMode)

    if toolCommonArgConfig.ProcessRemainingArgs:
        parser.add_argument("RemainingArgs", nargs=argparse.REMAINDER)
    if toolCommonArgConfig.SupportBuildTime:
        parser.add_argument("--BuildTime", action="store_true", help="Time the build")
    if toolCommonArgConfig.AddBuildFiltering or toolCommonArgConfig.AddUseFeatures:
        parser.add_argument(
            "--UseFeatures",
            default=DefaultValue.UseFeatures,
            help="The list of available features to build for. For example [OpenGLES2,OpenGLES3.1] or * for everything",
        )
    if toolCommonArgConfig.AddBuildFiltering:
        parser.add_argument(
            "--RequireFeatures",
            default=DefaultValue.RequireFeatures,
            help="The list of features that are required for a executable to be build. For example [OpenGLES2] to build all executables that use OpenGLES2.",
        )
        parser.add_argument(
            "--UseExtensions",
            default=DefaultValue.UseExtensions,
            help="The list of available extensions to build for. For example [OpenGLES3.1:EXT_geometry_shader,OpenGLES3.1:EXT_tessellation_shader] to allow the OpenGLES3.1 extensions EXT_geometry_shader and EXT_tessellation_shader. You can also specify * for all extensions (default).",
        )
        # parser.add_argument('--RequireExtensions', default=DefaultValue.RequireExtensionsList, help='The list of extensions that are required for a executable to be saved. For example [OpenGLES3.1:EXT_geometry_shader] to build all executables that use OpenGLES3.1:EXT_geometry_shader beware this allows OpenGLES3.2 apps that use EXT_geometry_shader since OpenGLES3.2 extends OpenGLES3.1.')
        parser.add_argument(
            "--Recipes",
            default=DefaultValue.Recipes,
            help="The modifies the list of recipes. For example [Recipe.gli_0_8_2_0,-Recipe.Vulkan] will enable the glm and disable the vulkan recipe.",
        )

    if toolCommonArgConfig.AddBuildFilteringExePackageName:
        parser.add_argument(
            "--ExePackageNameFilter",
            default=DefaultValue.ExePackageNameFilter,
            help="set a executable name filter, when set only exe packages that match the name filter will be build, if there a no executable package matches a error occurs",
        )

    if toolCommonArgConfig.AddBuildThreads:
        parser.add_argument(
            "--BuildThreads",
            default=str(DefaultValue.BuildThreads),
            help='Configures how many threads the builder should use, beware this is a hint that may be ignored by the builder, can be a number or "auto".',
        )

    if toolCommonArgConfig.AddGeneratorSelection:
        parser.add_argument(
            "-g",
            "--Generator",
            default=GeneratorNameString.Default,
            help="Select the generator to use (experimental feature): {}".format(", ".join(GeneratorNameString.AllStrings())),
        )
        parser.add_argument(
            "--CMakeGeneratorName",
            default=DefaultValue.CMakeGeneratorName,
            help="Select the cmake generator name.  Only used by the cmake generator. (experimental feature and work in progress argument name)",
        )
        parser.add_argument(
            "--CMakeBuildDir",
            default=DefaultValue.CMakeBuildDir,
            help="Select the build dir to use for cmake. Only used by the cmake generator. (experimental feature and work in progress argument name)",
        )
        parser.add_argument(
            "--CMakeBuildDirId",
            default=DefaultValue.CMakeBuildDirId,
            help="Add a postfix id to the auto generated build dir. This is a easy way to have multiple concurrent builds",
        )
        parser.add_argument(
            "--CMakeConfigArgs",
            default=DefaultValue.CMakeConfigArgs,
            help="Add additional cmake config command line arguments to the apps (not recipes).  Only used by the cmake generator. (experimental feature and work in progress argument name)",
        )
        parser.add_argument(
            "--CMakeConfigGlobalArgs",
            default=DefaultValue.CMakeConfigGlobalArgs,
            help="Add additional cmake config command line arguments to all cmake config commands.  Only used by the cmake generator. (experimental feature and work in progress argument name)",
        )
        parser.add_argument(
            "--CMakeInstallPrefix",
            default=DefaultValue.CMakeInstallPrefix,
            help="Select the install prefix for cmake. Only used by the cmake generator. (experimental feature and work in progress argument name)",
        )
        parser.add_argument(
            "--CMakeAllowFindPackage",
            default=DefaultValue.CMakeAllowFindPackage,
            help="Enable/disable the cmake find_package requests. (experimental feature and work in progress argument name)",
        )

    if toolCommonArgConfig.AddBuildVariants:
        parser.add_argument("--Variants", help="Configure the variants you wish to use for the build [WindowSystem=X11]")
        parser.add_argument("--debug", action="store_true", help="Configure the build as a debug build")
    if toolCommonArgConfig.AllowRecursive:
        parser.add_argument(
            "-r", "--recursive", action="store_true", help="From the current package location we scan all sub directories for packages and process them"
        )

    parser.add_argument("--set", action="append", help=f"Set a variable overriding all auto detection. Variables: {BuildVariables.GetSetableVariables()}")

    return parser


def __OnErrorInfo(buildTiming: BuildTimer | None, errorHelpManager: ErrorHelpManager) -> None:
    if buildTiming is not None:
        PrintBuildTiming(buildTiming)
    if len(errorHelpManager.OnErrorWarningHints) > 0:
        print("*** The following hints might help solve the issue but they also might be completely unrelated ***")
        for entry in errorHelpManager.OnErrorWarningHints:
            print(f"- HINT: {entry}")


def __RunStandalone(
    appFlowFactory: AToolAppFlowFactory, strToolAppTitle: str, toolCommonArgConfig: ToolCommonArgConfig, lowLevelToolConfig: LowLevelToolConfig
) -> None:
    log = Log(strToolAppTitle, lowLevelToolConfig.VerbosityLevel, showAppTitleIfVerbose=True)

    pluginConfigContext = PluginConfig.InitPluginConfigContext(log, CurrentVersion, lowLevelToolConfig.AllowDevelopmentPlugins)
    generatorIds = __PrepareGeneratorPlugins(pluginConfigContext, lowLevelToolConfig, toolCommonArgConfig)

    ### Do the actual command line parsing
    parser = __CreateParser(toolCommonArgConfig, True)
    if toolCommonArgConfig.AddPlatformArg:
        parser.add_argument("-p", "--platform", required=True, help="Select build platform: {}".format(", ".join(generatorIds)))

    buildTiming = None
    errorHelpManager = ErrorHelpManager()
    try:
        userTag = appFlowFactory.CreateStandaloneUserTag()
        appFlowFactory.AddCustomStandaloneArguments(parser, userTag)

        args = parser.parse_args()
        userSetVariables = __ParseUserSetVariables(log, args.set)

        if toolCommonArgConfig.ProcessRemainingArgs:
            args.RemainingArgs = __ProcessRemainingArgs(args.RemainingArgs)
        if toolCommonArgConfig.SupportBuildTime and args.BuildTime:
            buildTiming = BuildTimer()

        currentDir = lowLevelToolConfig.CurrentDir

        toolAppConfig = __CreateToolAppConfig(args, args.platform, toolCommonArgConfig, 0, userSetVariables)
        toolAppContext = ToolAppContext(log, errorHelpManager, lowLevelToolConfig, toolAppConfig, pluginConfigContext)
        toolAppFlow = appFlowFactory.Create(toolAppContext)

        toolAppFlow.ProcessFromStandaloneCommandLine(args, currentDir, userTag)

        if buildTiming:
            PrintBuildTiming(buildTiming)
    except GroupedException as ex:
        __OnErrorInfo(buildTiming, errorHelpManager)
        for entry in ex.ExceptionList:
            print(f"ERROR: {entry}")
        if lowLevelToolConfig.DebugEnabled:
            raise
        for entry in ex.ExceptionList:
            if isinstance(entry, ExitException):
                sys.exit(entry.ExitCode)
        sys.exit(1)
    except AggregateException as ex:
        __OnErrorInfo(buildTiming, errorHelpManager)
        for entry in ex.ExceptionList:
            print(f"ERROR: {entry}")
        if lowLevelToolConfig.DebugEnabled:
            if len(ex.ExceptionList) > 0:
                raise ex.ExceptionList[0] from ex
            raise
        for entry in ex.ExceptionList:
            if isinstance(entry, ExitException):
                sys.exit(entry.ExitCode)
        sys.exit(1)
    except ExitException as ex:
        __OnErrorInfo(buildTiming, errorHelpManager)
        sys.exit(ex.ExitCode)
    except Exception as ex:
        __OnErrorInfo(buildTiming, errorHelpManager)
        print(f"ERROR: {ex}")
        if lowLevelToolConfig.DebugEnabled:
            raise
        sys.exit(1)


def DetectBuildPlatform() -> str:
    return PlatformUtil.DetectBuildPlatform().lower()


def __Run(
    appFlowFactory: AToolAppFlowFactory,
    strToolAppTitle: str,
    toolCommonArgConfig: ToolCommonArgConfig,
    lowLevelToolConfig: LowLevelToolConfig,
    allowStandaloneMode: bool,
) -> None:
    log = Log(strToolAppTitle, lowLevelToolConfig.VerbosityLevel, showAppTitleIfVerbose=True)

    pluginConfigContext = PluginConfig.InitPluginConfigContext(log, CurrentVersion, lowLevelToolConfig.AllowDevelopmentPlugins)
    generatorIds = __PrepareGeneratorPlugins(pluginConfigContext, lowLevelToolConfig, toolCommonArgConfig)

    try:
        defaultPlatform = DetectBuildPlatform()
    except Exception as ex:
        print(f"ERROR: {ex}")
        if lowLevelToolConfig.DebugEnabled:
            raise
        sys.exit(1)

    ### Do the actual command line parsing
    parser = __CreateParser(toolCommonArgConfig, allowStandaloneMode)
    if toolCommonArgConfig.AddPlatformArg:
        parser.add_argument("-p", "--platform", default=defaultPlatform, help="Select build platform: {}".format(", ".join(generatorIds)))
    if toolCommonArgConfig.AllowForceClaimInstallArea:
        parser.add_argument(
            "--ForceClaimInstallArea",
            action="store_true",
            help="Override the security checks on the install area allowing us to use it even though its not empty. This means the existing content can be lost.",
        )

    # parser.add_argument('--NativeGen', action='store_true',  help='Force use the native build generator')

    toolConfig = None
    baseConfig = None
    try:
        basicConfig = BasicConfig(log)
        currentDir = lowLevelToolConfig.CurrentDir

        # Try to locate a project root configuration file
        projectRootConfig = GetProjectRootConfig(lowLevelToolConfig, basicConfig, currentDir)
        toolConfigFile = projectRootConfig.ToolConfigFile

        # Get the path to the toolconfig file if necessary and load the tool config file
        buildPlatformType = PlatformUtil.DetectBuildPlatformType()
        toolConfigPath = __GetToolConfigPath(toolConfigFile)
        toolConfig = ToolConfig(lowLevelToolConfig, buildPlatformType, CurrentVersion, basicConfig, toolConfigPath, projectRootConfig)
        baseConfig = BaseConfig(log, toolConfig)
    except Exception as ex:
        print(f"ERROR: {ex}")
        if lowLevelToolConfig.DebugEnabled:
            raise
        sys.exit(1)

    buildTiming = None
    errorHelpManager = ErrorHelpManager()
    try:
        defaultVSVersion = toolConfig.GetVisualStudioDefaultVersion()
        # if toolCommonArgConfig.AllowVSVersion:
        parser.add_argument(
            "--VSVersion",
            default=str(defaultVSVersion),
            help=f"Choose a specific visual studio version (2019,2022), This project defaults to: {defaultVSVersion}",
        )

        userTag = appFlowFactory.CreateUserTag(baseConfig)
        appFlowFactory.AddCustomArguments(parser, toolConfig, userTag)

        args = parser.parse_args()
        userSetVariables = __ParseUserSetVariables(log, args.set)

        # if toolCommonArgConfig.AllowVSVersion:
        pluginConfigContext.SetVSVersion(args.VSVersion)

        # if toolCommonArgConfig.AddPlatformArg and args.platform.lower() != PluginSharedValues.PLATFORM_ID_ALL:
        # PluginConfig.SetForceUseNativeGenerator(True)
        # PluginConfig.SetForceUseNativeGenerator(args.NativeGen)

        if toolCommonArgConfig.ProcessRemainingArgs:
            args.RemainingArgs = __ProcessRemainingArgs(args.RemainingArgs)
        if toolCommonArgConfig.SupportBuildTime and args.BuildTime:
            buildTiming = BuildTimer()

        if toolCommonArgConfig.AddBuildFiltering and args.Recipes == DefaultValue.Recipes and projectRootConfig.XmlExperimental is not None:
            tmpResult = projectRootConfig.XmlExperimental.TryGetRecipesDefaultValue(defaultPlatform)
            if tmpResult is not None:
                args.Recipes = f"[{tmpResult}]"

        toolAppConfig = __CreateToolAppConfig(args, defaultPlatform, toolCommonArgConfig, defaultVSVersion, userSetVariables)
        toolAppContext = ToolAppContext(log, errorHelpManager, lowLevelToolConfig, toolAppConfig, pluginConfigContext)
        toolAppFlow = appFlowFactory.Create(toolAppContext)
        toolAppFlow.ProcessFromCommandLine(args, currentDir, toolConfig, userTag)

        if buildTiming:
            PrintBuildTiming(buildTiming)
    except GroupedException as ex:
        __OnErrorInfo(buildTiming, errorHelpManager)
        for entry in ex.ExceptionList:
            print(f"ERROR: {entry}")
        if lowLevelToolConfig.DebugEnabled:
            raise
        for entry in ex.ExceptionList:
            if isinstance(entry, ExitException):
                sys.exit(entry.ExitCode)
        sys.exit(1)
    except AggregateException as ex:
        __OnErrorInfo(buildTiming, errorHelpManager)
        for entry in ex.ExceptionList:
            print(f"ERROR: {entry}")
        if lowLevelToolConfig.DebugEnabled:
            if len(ex.ExceptionList) > 0:
                raise ex.ExceptionList[0] from ex
            raise
        for entry in ex.ExceptionList:
            if isinstance(entry, ExitException):
                sys.exit(entry.ExitCode)
        sys.exit(1)
    except ExitException as ex:
        __OnErrorInfo(buildTiming, errorHelpManager)
        sys.exit(ex.ExitCode)
    except Exception as ex:
        __OnErrorInfo(buildTiming, errorHelpManager)
        print(f"ERROR: {ex}")
        if lowLevelToolConfig.DebugEnabled:
            raise
        sys.exit(1)


def Run(appFlowFactory: AToolAppFlowFactory, allowStandaloneMode: bool = False) -> None:
    strToolAppTitle = f"{appFlowFactory.GetTitle()} V{CurrentVersion}"
    appShortDesc = appFlowFactory.GetShortDesc()
    if appShortDesc is not None:
        strToolAppTitle = f"{strToolAppTitle} - {appShortDesc}"

    toolCommonArgConfig = appFlowFactory.GetToolCommonArgConfig()

    lowLevelToolConfig = __EarlyArgumentParser(allowStandaloneMode)
    if lowLevelToolConfig is None:
        sys.exit(1)

    if not lowLevelToolConfig.ProfilerEnabled:
        if not lowLevelToolConfig.StandaloneEnabled:
            __Run(appFlowFactory, strToolAppTitle, toolCommonArgConfig, lowLevelToolConfig, allowStandaloneMode)
        else:
            __RunStandalone(appFlowFactory, strToolAppTitle, toolCommonArgConfig, lowLevelToolConfig)
    else:
        try:
            import cProfile
            import pstats

            if not lowLevelToolConfig.StandaloneEnabled:
                cProfile.runctx(
                    "__Run(appFlowFactory, strToolAppTitle, toolCommonArgConfig, lowLevelToolConfig, allowStandaloneMode)", globals(), locals(), "restats"
                )
            else:
                cProfile.runctx("__RunStandalone(appFlowFactory, strToolAppTitle, toolCommonArgConfig, lowLevelToolConfig)", globals(), locals(), "restats")
            p = pstats.Stats("restats")
            p.strip_dirs().sort_stats(-1)
            p.print_stats()
            p.sort_stats("cumulative")
            p.print_stats(10)
            p.sort_stats("time")
            p.print_stats(10)
            p.sort_stats("tottime")
            p.print_stats(100)
        except ImportError as ex:
            raise Exception("Standard python package cProfile or pstats is not available. So profiling is not available.") from ex
