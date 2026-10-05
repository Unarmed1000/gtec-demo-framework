#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2026, Mana Battery ApS
# * All rights reserved.
# *
# * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
# *
# * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
# * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
# *    documentation and/or other materials provided with the distribution.
# * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
# *    software without specific prior written permission.
# *
# * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# ****************************************************************************************************************************************************

# The CMakePresets.json of a generated CMake project: collects what the content is made from and writes the file next to the top-level
# CMakeLists.txt (the content itself is made by Generator/CMakePresets.py).
#
# The configure presets come from the configure command the builder runs: the command of the generator's config report, resolved by the
# code the builder resolves it with (Generator/Report/ResolvedConfigCommand.py), once for each option of the config variant.
#
# - A multi-config generator (Visual Studio) has one build directory for every configuration, so the project gets a preset for every
#   option of the config variant. Any other generator has the configuration in its configure command, so the project gets the presets
#   of the configuration it was generated for. A generator the tool does not know is one of those: its configure command names the
#   configuration and so does its build (see BuildExternal/CMakeGeneratorKind.py), so its build preset names it too.
# - No file is written when the presets can not say what the command does, or when the cmake of the machine is too old for the format.
#   A file the tool wrote in an earlier generation is removed then, as it describes another configure.
# - A file the tool did not write (it has no vendor entry of the tool) is never written to and never removed.
# - The file is not one of the generated files the configure cache of the builder looks at: writing it does not cause a configure.
# - Nothing is scanned and no process is started for it: a generation must not get slower.

from collections.abc import Sequence

from FslBuildGen import IOUtil, TemplateIO
from FslBuildGen.BuildConfig.BuildUtil import BuildUtil
from FslBuildGen.BuildExternal import CMakeGeneratorKind
from FslBuildGen.DataTypes import BuildVariantConfig, PackageType
from FslBuildGen.Engine.PackageFlavorOptionName import PackageFlavorOptionName
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavorName import UnresolvedPackageFlavorName
from FslBuildGen.ExternalVariantConstraints import ExternalVariantConstraints
from FslBuildGen.Generator import CMakeGeneratorUtil, CMakePresets
from FslBuildGen.Generator.GeneratorCMake import GeneratorCMake, LocalMagicBuildVariants
from FslBuildGen.Generator.GeneratorCMakeConfig import GeneratorCMakeConfig
from FslBuildGen.Generator.Report import ResolvedConfigCommand
from FslBuildGen.Generator.Report.ReportVariableFormatter import ReportVariableFormatter
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package
from FslBuildGen.SharedGeneration import ToolAddedVariant
from FslBuildGen.ToolConfig import ToolConfig


class _TopLevelProject:
    """The top-level package of a generation and the directory of the CMakeLists.txt the generator writes for it"""

    def __init__(self, package: Package, directory: str) -> None:
        super().__init__()
        self.Package = package
        self.Directory = directory


def _TryGetTopLevelProject(toolConfig: ToolConfig, cmakeBuildPackageDir: str, packages: Sequence[Package]) -> _TopLevelProject | None:
    """The project the configure command names, None when the generator writes no top-level CMakeLists.txt for it from these packages"""
    topProjectContext = toolConfig.ProjectInfo.TopProjectContext
    for package in packages:
        if package.Type == PackageType.TopLevel:
            # GeneratorCMake writes a top-level file for each project that has a package in the build
            if any(projectContext.ProjectId == topProjectContext.ProjectId for projectContext in CMakeGeneratorUtil.GetProjectContexts(package)):
                return _TopLevelProject(package, GeneratorCMake._GetProjectDirectoryName(cmakeBuildPackageDir, topProjectContext))
            return None
    return None


def _WithConfigOption(variantConstraints: ExternalVariantConstraints, configOption: str) -> ExternalVariantConstraints:
    """The variant constraints of the generation with the config variant set to the option"""
    constraintsDict = dict(variantConstraints.Dict)
    constraintsDict[UnresolvedPackageFlavorName(ToolAddedVariant.CONFIG)] = PackageFlavorOptionName(configOption)
    return ExternalVariantConstraints(constraintsDict)


def _GetPresetConfigOptions(cmakeConfig: GeneratorCMakeConfig, configVariantOptions: Sequence[str], hasEveryConfiguration: bool) -> list[str]:
    if hasEveryConfiguration:
        return list(configVariantOptions)
    # The configuration is part of the configure command: the project is the one of the configuration it was generated for
    activeName = BuildVariantConfig.ToString(cmakeConfig.BuildVariantConfig)
    return [option for option in configVariantOptions if option.lower() == activeName]


def _CollectConfigurations(
    log: Log,
    toolConfig: ToolConfig,
    platformName: str,
    cmakeConfig: GeneratorCMakeConfig,
    cmakeBuildPackageDir: str,
    topLevelPackage: Package,
    configVariantOptions: Sequence[str],
    variantConstraints: ExternalVariantConstraints,
) -> list[CMakePresets.PresetConfiguration]:
    """The configure command of each configuration, made and resolved the way the builder does it (Builder.BuildPackages, __ConfigureBuild)"""
    allConfigOptions = list(configVariantOptions)
    configReport = GeneratorCMake._GenerateConfigReport(log, toolConfig, platformName, cmakeConfig, cmakeBuildPackageDir, topLevelPackage)
    variableReport = GeneratorCMake._GenerateConfigVariableReport(log, topLevelPackage, allConfigOptions)
    BuildUtil.AddCustomVariables(variableReport, toolConfig.ProjectInfo)

    # The same questions the configure command and the build command ask (GeneratorCMake): without the configuration in the
    # configure command the build directory holds every configuration, and the build names it ('--config') unless the generator is
    # known not to listen
    hasEveryConfiguration = not CMakeGeneratorKind.IsConfigurationGivenAtConfigure(cmakeConfig.GeneratorName)
    isConfigurationGivenAtBuild = CMakeGeneratorKind.IsConfigurationGivenAtBuild(cmakeConfig.GeneratorName)
    configurations: list[CMakePresets.PresetConfiguration] = []
    for configOption in _GetPresetConfigOptions(cmakeConfig, allConfigOptions, hasEveryConfiguration):
        constraints = _WithConfigOption(variantConstraints, configOption)
        resolved = ResolvedConfigCommand.Resolve(configReport.ConfigCommandReport, variableReport, constraints)
        buildConfiguration = None
        if isConfigurationGivenAtBuild:
            buildConfiguration = ReportVariableFormatter.Format(f"${{{LocalMagicBuildVariants.CMakeBuildConfig}}}", variableReport, constraints)
        configurations.append(CMakePresets.PresetConfiguration(configOption, buildConfiguration, resolved.Command, resolved.CurrentWorkingDirectory))
    return configurations


def _CollectEnvironment(toolConfig: ToolConfig, packages: Sequence[Package]) -> dict[str, str]:
    """The environment variables the generated CMakeLists.txt files read, with the value each has now. One that is not set is left out."""
    allPackages = list(packages)
    uniqueEnvironmentVariables = CMakeGeneratorUtil.ExtractUniqueVariables(allPackages)[1]
    toolProjectContextsDict = {projectContext.ProjectId: projectContext for projectContext in toolConfig.ProjectInfo.Contexts}
    names: set[str] = set()
    for projectId in {package.ProjectContext.ProjectId for package in allPackages if package.Type != PackageType.TopLevel}:
        toolProjectContext = toolProjectContextsDict.get(projectId)
        if toolProjectContext is not None:
            # As GeneratorCMake asks for the variables of a CMakeLists.txt
            names.update(CMakeGeneratorUtil.GetRootDirectoryEnvironmentVariableNames(toolConfig, toolProjectContext, True, uniqueEnvironmentVariables))

    environment: dict[str, str] = {}
    for name in names:
        value = IOUtil.TryGetEnvironmentVariable(name)
        if value is not None:
            environment[name] = value
    return environment


def _CollectInputOf(
    project: _TopLevelProject,
    log: Log,
    toolConfig: ToolConfig,
    platformName: str,
    cmakeConfig: GeneratorCMakeConfig,
    cmakeBuildPackageDir: str,
    packages: Sequence[Package],
    configVariantOptions: Sequence[str],
    variantConstraints: ExternalVariantConstraints,
) -> CMakePresets.PresetsInput:
    try:
        configurations = _CollectConfigurations(
            log, toolConfig, platformName, cmakeConfig, cmakeBuildPackageDir, project.Package, configVariantOptions, variantConstraints
        )
    except Exception as ex:
        # What stops the builder from making its configure command (a variant option that does not exist, a recipe path it can not pass
        # on) is an error of the build. The presets are a by-product of the generation, which must not fail where it did not fail before.
        raise CMakePresets.CanNotBeExpressedException(f"the configure command could not be made ({ex})") from ex

    version = cmakeConfig.CMakeVersion
    # The config variant is not listed, it is what the presets of a project differ by
    variants = [(name.Value, option.Value) for name, option in variantConstraints.Dict.items() if name.Value != ToolAddedVariant.CONFIG]
    variants.sort()
    return CMakePresets.PresetsInput(
        (version.Major, version.Minor, version.Build),
        cmakeConfig.CMakeCommand,
        project.Directory,
        platformName,
        configurations,
        _CollectEnvironment(toolConfig, packages),
        variants,
    )


def CollectInput(
    log: Log,
    toolConfig: ToolConfig,
    platformName: str,
    cmakeConfig: GeneratorCMakeConfig,
    cmakeBuildPackageDir: str,
    packages: Sequence[Package],
    configVariantOptions: Sequence[str],
    variantConstraints: ExternalVariantConstraints,
) -> CMakePresets.PresetsInput | None:
    """What the presets of the generated project are made from, None when the generator writes no top-level CMakeLists.txt for the packages.
    Raises CMakePresets.CanNotBeExpressedException when the configure command can not be made.
    """
    project = _TryGetTopLevelProject(toolConfig, cmakeBuildPackageDir, packages)
    if project is None:
        return None
    return _CollectInputOf(project, log, toolConfig, platformName, cmakeConfig, cmakeBuildPackageDir, packages, configVariantOptions, variantConstraints)


def _IsWrittenByTheTool(content: bytes) -> bool:
    try:
        return CMakePresets.IsWrittenByTheTool(content.decode("utf-8"))
    except UnicodeDecodeError:
        return False


def Update(
    log: Log,
    toolConfig: ToolConfig,
    platformName: str,
    cmakeConfig: GeneratorCMakeConfig,
    cmakeBuildPackageDir: str,
    packages: Sequence[Package],
    configVariantOptions: Sequence[str],
    variantConstraints: ExternalVariantConstraints,
    disableWrite: bool,
) -> None:
    """Write the presets of the project the CMake generator just wrote for the packages, or remove the ones of an earlier generation when
    this project can not have any. Does nothing when nothing may be written.
    """
    if disableWrite:
        return
    project = _TryGetTopLevelProject(toolConfig, cmakeBuildPackageDir, packages)
    if project is None:
        return

    try:
        source = _CollectInputOf(project, log, toolConfig, platformName, cmakeConfig, cmakeBuildPackageDir, packages, configVariantOptions, variantConstraints)
        result = CMakePresets.TryCreate(source)
    except CMakePresets.CanNotBeExpressedException as ex:
        result = CMakePresets.PresetsResult(None, str(ex))

    filename = IOUtil.Join(project.Directory, CMakePresets.FileName)
    existingContent = IOUtil.TryReadBinaryFile(filename)
    isForeignFile = existingContent is not None and not _IsWrittenByTheTool(existingContent)
    if result.Content is None:
        log.LogPrintVerbose(2, f"No CMake presets for the generated project: {result.Reason}")
        if isForeignFile:
            log.LogPrintVerbose(2, f"'{filename}' was not written by the tool, it is left as it is")
        elif existingContent is not None:
            log.LogPrintVerbose(2, f"Removing '{filename}', the presets of an earlier generation")
            IOUtil.RemoveFile(filename)
        return
    if isForeignFile:
        log.LogPrintVerbose(2, f"No CMake presets for the generated project: '{filename}' was not written by the tool, it is left as it is")
        return
    TemplateIO.WriteGeneratedFileIfChanged(filename, CMakePresets.ToText(result.Content))
