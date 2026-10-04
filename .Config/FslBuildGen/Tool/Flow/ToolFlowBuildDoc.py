#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright 2017, 2024 NXP
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

# from typing import Callable
from typing import Any, cast

from FslBuildGen import IOUtil, MarkdownIO, PluginSharedValues, TextFileReader
from FslBuildGen import Main as MainFlow
from FslBuildGen.BasicConfig import BasicConfig
from FslBuildGen.Build.BuildOutcome import BuildOutcome
from FslBuildGen.Build.BuildVariantConfigUtil import BuildVariantConfigUtil
from FslBuildGen.Build.ForAllConfig import ForAllConfig
from FslBuildGen.BuildConfig.BuildDocConfiguration import BuildDocConfiguration
from FslBuildGen.BuildConfig.BuildUtil import BuildUtil
from FslBuildGen.BuildDoc import ArgumentExtraction, ArgumentExtractionState
from FslBuildGen.Config import Config
from FslBuildGen.Context.GeneratorContext import GeneratorContext
from FslBuildGen.DataTypes import PackageType
from FslBuildGen.Engine.EngineResolveConfig import EngineResolveConfig
from FslBuildGen.Exceptions import BuildConfigureFailedException, ExitException, UsageErrorException
from FslBuildGen.Generator.GeneratorPlugin import GeneratorPlugin
from FslBuildGen.Location.ResolvedPath import ResolvedPath
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package
from FslBuildGen.Packages.PackageRequirement import PackageRequirement
from FslBuildGen.PlatformUtil import PlatformUtil
from FslBuildGen.Tool.AToolAppFlow import AToolAppFlow
from FslBuildGen.Tool.AToolAppFlowFactory import AToolAppFlowFactory
from FslBuildGen.Tool.Flow import ToolFlowBuild
from FslBuildGen.Tool.ToolAppConfig import ToolAppConfig
from FslBuildGen.Tool.ToolAppContext import ToolAppContext
from FslBuildGen.Tool.ToolCommonArgConfig import ToolCommonArgConfig
from FslBuildGen.ToolConfig import ToolConfig
from FslBuildGen.ToolConfigProjectContext import ToolConfigProjectContext
from FslBuildGen.ToolConfigRootDirectory import ToolConfigRootDirectory
from FslBuildGen.VariableContextHelper import VariableContextHelper

JsonDictType = dict[str, Any]


def ExtractPackages(packages: list[Package], packageType: PackageType) -> list[Package]:
    res: list[Package] = []
    for package in packages:
        if package.Type == packageType:
            res.append(package)
    return res


def IndexOf(lines: list[str], magicCommentContent: str) -> int:
    actualSearchString = f"<!-- #{magicCommentContent}# -->"
    for idx, val in enumerate(lines):
        if val.strip() == actualSearchString:
            return idx
    return -1


def RightStripLines(lines: list[str]) -> None:
    for index, entry in enumerate(lines):
        lines[index] = entry.rstrip()


def TryReplaceSection(basicConfig: BasicConfig, lines: list[str], sectionName: str, replacementLines: list[str], path: str) -> list[str] | None:
    sectionBegin = f"{sectionName}_BEGIN"
    sectionEnd = f"{sectionName}_END"
    startIndex = IndexOf(lines, sectionBegin)
    if startIndex < 0:
        basicConfig.LogPrint(f"WARNING: {path} did not contain a {sectionBegin} section")
        return None
    endIndex = IndexOf(lines, sectionEnd)
    if endIndex < 0:
        basicConfig.LogPrint(f"WARNING: {path} did not contain a {sectionEnd} section")
        return None
    if startIndex >= endIndex:
        basicConfig.LogPrint(f"WARNING: {path} {sectionBegin} must appear before {sectionEnd}")
        return None

    start = lines[0 : startIndex + 1]
    end = lines[endIndex:]
    return start + replacementLines + end


def TocEntryName(line: str) -> str:
    return line.strip()


def TocEntryLink(line: str) -> str:
    line = line.strip()
    line = line.replace(" ", "-")
    line = line.replace(".", "")
    line = line.replace(":", "")
    line = line.lower()
    return line


def TryTocPrepareLine(line: str) -> str | None:
    line = line.strip()
    if line.startswith("["):
        index = line.find("]")
        if index == -1:
            return None
        return line[1:index]
    return line


def BuildTableOfContents(lines: list[str], depth: int) -> list[str]:
    # Dumb but simple TOC genration
    startIndex = IndexOf(lines, "AG_TOC_END")
    if startIndex < 0:
        startIndex = 0

    tocLines = []
    for index in range(startIndex, len(lines)):
        line = lines[index]
        if line.startswith("# "):
            line = line[2:]
            resLine = TryTocPrepareLine(line)
            if resLine is not None:
                tocLines.append(f"* [{TocEntryName(resLine)}](#{TocEntryLink(resLine)})")
        elif line.startswith("## ") and depth >= 2:
            line = line[3:]
            resLine = TryTocPrepareLine(line)
            if resLine is not None:
                tocLines.append(f"  * [{TocEntryName(resLine)}](#{TocEntryLink(resLine)})")
        elif line.startswith("### ") and depth >= 3:
            line = line[4:]
            resLine = TryTocPrepareLine(line)
            if resLine is not None:
                tocLines.append(f"    * [{TocEntryName(resLine)}](#{TocEntryLink(resLine)})")
        elif line.startswith("#### ") and depth >= 4:
            line = line[5:]
            resLine = TryTocPrepareLine(line)
            if resLine is not None:
                tocLines.append(f"      * [{TocEntryName(resLine)}](#{TocEntryLink(resLine)})")
    return tocLines


def TryInsertTableOfContents(basicConfig: BasicConfig, lines: list[str], depth: int, path: str) -> list[str] | None:
    tocLines = BuildTableOfContents(lines, depth)
    return TryReplaceSection(basicConfig, lines, "AG_TOC", tocLines, path)


# AG_DEMOAPP_HEADER_BEGIN
def BuildDemoAppHeader(package: Package) -> list[str]:
    result: list[str] = []
    result.append(f"# {package.NameInfo.ShortName.Value}")
    #    result.append('<img src="./Example.jpg" height="135px" style="float:right">')
    result.append('<img src="Example.jpg" height="135px">')
    result.append("")
    return result


def TryLoadTextFileAsLines(log: Log, path: str) -> list[str] | None:
    result = MarkdownIO.TryReadMarkdownFile(log, path)
    if result is None:
        return None
    lines = result.split("\n")
    return lines


def TryLoadReadMe(log: Log, path: str) -> list[str] | None:
    lines = TryLoadTextFileAsLines(log, path)
    if lines is None:
        return None

    RightStripLines(lines)
    return lines


class OptionArgument:
    OptionNone = 0
    # The option requires a argument
    OptionRequired = 1


class OptionGroup:
    Default = 0x00000001
    Host = 0x00000002
    Demo = 0x00000004
    Custom0 = 0x00001000
    Custom1 = 0x00002000
    Custom2 = 0x00004000
    Custom3 = 0x00008000
    Custom4 = 0x00010000
    Custom5 = 0x00020000
    Custom6 = 0x00040000
    Custom7 = 0x00080000
    Custom8 = 0x00100000
    Custom9 = 0x00200000
    Hidden = 0x40000000


class ProgramArgument:
    def __init__(self, basicConfig: BasicConfig, entry: dict[str, Any]) -> None:
        super().__init__()
        self.SourceName = self.__ReadEntry(entry, "SourceName")
        self.Description = self.__ReadEntry(entry, "Description")
        self.Group = self.__ReadOptionGroup(entry, "Group")
        self.HasArg = self.__ReadOptionArgument(entry, "HasArg")
        self.IsPositional = self.__ReadEntry(entry, "IsPositional")
        self.Name = self.__ReadEntry(entry, "Name", "")
        self.ShortName = self.__ReadEntry(entry, "ShortName", "")
        self.Type = self.__ReadEntry(entry, "Type", "")
        self.Help_FormattedName = self.__ReadEntry(entry, "Help_FormattedName", "")

        strEnding = "OptionParser"
        if self.SourceName.endswith(strEnding):
            self.SourceName = self.SourceName[0 : -len(strEnding)]

    def __filterNonPrintable(self, strSrc: str) -> str:
        return "".join([c for c in strSrc if ord(c) > 31 or ord(c) == 9])

    def __ReadEntry(self, srcDict: dict[str, Any], name: str, defaultValue: str | None = None) -> str:
        if name in srcDict:
            return self.__filterNonPrintable(srcDict[name].strip())
        elif defaultValue is not None:
            return defaultValue
        raise Exception(f"command line argument entry missing attribute: '{name}'")

    def __TryReadEntry(self, srcDict: dict[Any, str], name: str, defaultValue: str | None = None) -> str | None:
        if name in srcDict:
            return self.__filterNonPrintable(srcDict[name].strip())
        return defaultValue

    def __ReadOptionArgument(self, srcDict: dict[str, Any], name: str) -> int:
        result = self.__ReadEntry(srcDict, name)
        if result == "0":
            return OptionArgument.OptionNone
        elif result == "1":
            return OptionArgument.OptionRequired
        else:
            raise Exception(f"command line argument OptionArgument '{name}' has unsupported value of '{result}'")

    def __ReadOptionGroup(self, srcDict: dict[str, Any], name: str) -> int:
        result = self.__ReadEntry(srcDict, name)
        return int(result)


def DecodeJsonArgumentList(basicConfig: BasicConfig, arguments: list[dict[str, Any]]) -> list[ProgramArgument]:
    res: list[ProgramArgument] = []
    for entry in arguments:
        res.append(ProgramArgument(basicConfig, entry))
    return res


def GetMaxFormattedNameLength(entries: list[ProgramArgument]) -> int:
    count = 0
    for entry in entries:
        if len(entry.Help_FormattedName) > count:
            count = len(SafeMarkdownString(entry.Help_FormattedName))
    return count


def GetMaxDescriptionLength(entries: list[ProgramArgument]) -> int:
    count = 0
    for entry in entries:
        if len(entry.Description) > count:
            count = len(SafeMarkdownString(entry.Description))
    return count


def GetMaxSourceNameLength(entries: list[ProgramArgument]) -> int:
    count = 0
    for entry in entries:
        if len(entry.SourceName) > count:
            count = len(SafeMarkdownString(entry.Description))
    return count


def SafeMarkdownString(strSrc: str) -> str:
    return strSrc.replace("<", "\\<").replace("|", "\\|")


# def GroupArgumentsBySourceName(basicConfig, arguments):
#    dict = {}
#    for entry in arguments:
#        if not entry.SourceName in dict:
#            dict[entry.SourceName] = []
#        dict[entry.SourceName].append(entry)

#    for entry in dict.values():
#        entry.sort(key=lambda s: s.Help_FormattedName.lower())
#    return dict


def TryBuildArgumentTableLines(basicConfig: BasicConfig, argumentDict: JsonDictType) -> list[str] | None:
    mainKey = "arguments"
    if mainKey not in argumentDict:
        return None

    srcArguments: list[dict[str, Any]] = argumentDict[mainKey]
    arguments: list[ProgramArgument] = DecodeJsonArgumentList(basicConfig, srcArguments)

    maxNameLength = GetMaxFormattedNameLength(arguments)
    maxDescLength = GetMaxDescriptionLength(arguments)
    maxSourceNameLength = GetMaxSourceNameLength(arguments)

    arguments.sort(key=lambda s: (s.SourceName, s.Help_FormattedName.lower()))

    # groupedArgumentDict = GroupArgumentsBySourceName(basicConfig, arguments)

    # sortedKeys = list(arguments.keys())
    # sortedKeys.sort()

    # Command line arguments:
    #
    # Argument            |Description
    # --------------------|---

    formatString = f"{{0:<{maxNameLength}}}|{{1:<{maxDescLength}}}|{{2}}"

    result: list[str] = []

    #    for key, argumentList in groupedArgumentDict.items():
    result.append("")
    result.append(g_argumentHeading)
    result.append("")
    result.append(formatString.format("Argument", "Description", "Source"))
    result.append("{}|{}|{}".format("-" * maxNameLength, "-" * maxDescLength, "-" * maxSourceNameLength))
    for entry in arguments:
        if entry.Group != OptionGroup.Hidden:
            result.append(
                formatString.format(SafeMarkdownString(entry.Help_FormattedName), SafeMarkdownString(entry.Description), SafeMarkdownString(entry.SourceName))
            )
    return result


# A one-time correction. The versions of the tool before 3.14.53 wrote the heading of the argument section with a stray quote. The section
# is only written again when the arguments of the app are extracted (the app is built and run), so a README.md would keep the old
# heading for years. A run that does not extract the arguments corrects that one line. It is done on the bytes of the file: nothing
# else in it changes, not the newlines, not the whitespace at the end of a line and not a byte order mark.
g_argumentSectionName = "AG_DEMOAPP_COMMANDLINE_ARGUMENTS"
g_argumentHeading = "Command line arguments:"
g_argumentHeadingOfOlderVersions = "Command line arguments':"


def TryCorrectArgumentHeadingOfOlderVersions(content: bytes) -> bytes | None:
    """The bytes of a README.md with the heading of its argument section corrected, None when there is nothing to correct: the file has
    no argument section (the two marker comments, found like TryReplaceSection finds them) or the section does not hold the old heading
    on a line of its own.
    """
    oldHeading = g_argumentHeadingOfOlderVersions.encode("ascii")
    if oldHeading not in content:
        return None
    # The lines with their newlines, so the file is put together again as it was
    lines = content.splitlines(keepends=True)
    strippedLines = [line.strip() for line in lines]
    sectionBegin = f"<!-- #{g_argumentSectionName}_BEGIN# -->".encode("ascii")
    sectionEnd = f"<!-- #{g_argumentSectionName}_END# -->".encode("ascii")
    if sectionBegin not in strippedLines or sectionEnd not in strippedLines:
        return None
    corrected = False
    for index in range(strippedLines.index(sectionBegin) + 1, strippedLines.index(sectionEnd)):
        if strippedLines[index] == oldHeading:
            lines[index] = lines[index].replace(oldHeading, g_argumentHeading.encode("ascii"))
            corrected = True
    return b"".join(lines) if corrected else None


def CorrectArgumentHeadingOfOlderVersions(config: Config, path: str) -> bool:
    """Correct the heading in the README.md at the path, returns true if the file has the old heading (a dry run does not write it)"""
    content = IOUtil.TryReadBinaryFile(path)
    correctedContent = TryCorrectArgumentHeadingOfOlderVersions(content) if content is not None else None
    if correctedContent is None:
        return False
    if config.DisableWrite:
        config.LogPrintVerbose(1, f"Would correct the heading of the command line arguments in '{path}'")
    else:
        config.LogPrintVerbose(1, f"Corrected the heading of the command line arguments in '{path}'")
        IOUtil.WriteBinaryFile(path, correctedContent)
    return True


def UpdatePackageReadMe(
    basicConfig: BasicConfig, package: Package, lines: list[str], packageArgumentsDict: dict[Package, JsonDictType], path: str
) -> list[str]:
    newHeader = BuildDemoAppHeader(package)
    lines2 = TryReplaceSection(basicConfig, lines, "AG_DEMOAPP_HEADER", newHeader, path)
    if lines2 is not None:
        lines = lines2
    if package in packageArgumentsDict:
        argumentsLines = TryBuildArgumentTableLines(basicConfig, packageArgumentsDict[package])
        if argumentsLines is not None:
            newLines = TryReplaceSection(basicConfig, lines, g_argumentSectionName, argumentsLines, path)
            if newLines is not None:
                lines = newLines
    return lines


def SaveReadMe(config: Config, path: str, lines: list[str]) -> None:
    text = "\n".join(lines)
    if not config.DisableWrite:
        MarkdownIO.WriteMarkdownFileIfChanged(path, text)


def TryExtractBrief(basicConfig: BasicConfig, lines: list[str], path: str) -> list[str] | None:
    startIndex = IndexOf(lines, "AG_BRIEF_BEGIN")
    if startIndex < 0:
        basicConfig.LogPrint(f"WARNING: {path} did not contain a AG_BRIEF_BEGIN section")
        return None
    endIndex = IndexOf(lines, "AG_BRIEF_END")
    if endIndex < 0:
        basicConfig.LogPrint(f"WARNING: {path} did not contain a AG_BRIEF_END section")
        return None
    if startIndex >= endIndex:
        basicConfig.LogPrint(f"WARNING: {path} AG_BRIEF_BEGIN must appear before AG_BRIEF_END")
        return None

    result = lines[startIndex + 1 : endIndex]

    # remove all empty lines at the end
    index = len(result) - 1
    while index >= 0 and len(result[index]) == 0:
        result.pop(index)
        index = index - 1

    return result


def ReadJsonFile(log: Log, filename: str) -> JsonDictType:
    """Read the file a demo app wrote its command line arguments to"""
    return cast(JsonDictType, ArgumentExtraction.ReadJsonFile(log, filename))


def GetGenFilePath(config: Config, package: Package) -> str:
    if package.AbsolutePath is None:
        raise Exception("Invalid package")
    return IOUtil.Join(package.AbsolutePath, config.GenFileName)


def __BuildAndRunApps(
    toolAppContext: ToolAppContext,
    config: Config,
    apps: list[Package],
    argumentFileDirectory: str,
    packageConfigurationType: str,
    runApps: list[Package] | None = None,
) -> BuildOutcome:
    """One FslBuild run for the apps: 'FslBuild --KeepGoing --ForAllExe "(EXE) --System.Arguments.Save <file of the app> -h"'.
    A build or a run that fails is in the outcome that is returned, a configure that fails raises BuildConfigureFailedException.
    runApps: only these apps are run, the other ones are only built. None runs all of them.
    """
    toolFlowConfig = ToolFlowBuild.GetDefaultLocalConfig()
    toolFlowConfig.SetToolAppConfigValues(toolAppContext.ToolAppConfig)
    toolFlowConfig.PackageConfigurationType = packageConfigurationType
    toolFlowConfig.KeepGoing = True
    toolFlowConfig.ForAllConfig = ForAllConfig.CreateForAllExeArgumentsConfig(
        ArgumentExtraction.GetRunArguments(argumentFileDirectory),
        ArgumentExtraction.RunTimeoutSeconds,
        None if runApps is None else [app.Name for app in runApps],
    )
    buildFlow = ToolFlowBuild.ToolFlowBuild(toolAppContext)
    genFiles = [GetGenFilePath(config, app) for app in apps]
    outcome = buildFlow.Build(IOUtil.GetDirectoryName(genFiles[0]), config.ToolConfig, toolFlowConfig, genFiles)
    if outcome is None:
        raise Exception("Internal error, the build of the apps returned no outcome")
    return outcome


def __ExtractWithOneBuild(
    toolAppContext: ToolAppContext, config: Config, apps: list[Package], runApps: list[Package], argumentFileDirectory: str, packageConfigurationType: str
) -> list[ArgumentExtraction.AppExtraction]:
    """apps are built with one build, which is the build of an earlier run of the same apps: nothing is configured again and what is built
    already costs nothing. runApps are the ones among them that are run, the result is their state.
    """
    runsAll = len(runApps) == len(apps)
    if runsAll:
        config.LogPrint(f"Building and running {ArgumentExtraction.FormatAppCount(len(apps))} with one build to extract the command line arguments")
    else:
        config.LogPrint(
            f"Building {ArgumentExtraction.FormatAppCount(len(apps))} with one build and running {len(runApps)} of them to extract the command line arguments"
        )
    try:
        outcome = __BuildAndRunApps(toolAppContext, config, apps, argumentFileDirectory, packageConfigurationType, None if runsAll else runApps)
    except BuildConfigureFailedException as ex:
        raise ArgumentExtraction.ConfigureFailedException(len(apps), ex.ExitCode) from ex
    return [ArgumentExtraction.CreateAppExtraction(config, app, outcome, argumentFileDirectory, ArgumentExtraction.RunTimeoutSeconds) for app in runApps]


def __ExtractWithOneBuildPerApp(
    toolAppContext: ToolAppContext, config: Config, runApps: list[Package], argumentFileDirectory: str, packageConfigurationType: str
) -> list[ArgumentExtraction.AppExtraction]:
    config.LogPrint(f"Building and running {ArgumentExtraction.FormatAppCount(len(runApps))} one at a time to extract the command line arguments")
    result: list[ArgumentExtraction.AppExtraction] = []
    for app in runApps:
        config.LogPrint(f"- Building and running {app.Name}")
        try:
            outcome = __BuildAndRunApps(toolAppContext, config, [app], argumentFileDirectory, packageConfigurationType)
        except BuildConfigureFailedException as ex:
            # Only this app is lost: the other ones have a build of their own
            reason = f"not built (the configure failed with exit code {ex.ExitCode})"
            result.append(ArgumentExtraction.AppExtraction(app, ArgumentExtraction.ExtractionState.NotBuilt, reason))
            continue
        result.append(ArgumentExtraction.CreateAppExtraction(config, app, outcome, argumentFileDirectory, ArgumentExtraction.RunTimeoutSeconds))
    return result


def GetArgumentFileDirectory(config: Config, generator: GeneratorPlugin) -> str:
    """Where the apps write their arguments: a directory in the cache directory of the build, which every generator has"""
    if generator.CMakeConfig is None:
        raise Exception("Internal error, the generator is not configured")
    return ArgumentExtraction.GetArgumentFileDirectory(BuildUtil.GetBuildDir(config.ToolConfig.ProjectInfo, generator.CMakeConfig.CacheDir))


def RunArgumentExtraction(
    toolAppContext: ToolAppContext,
    config: Config,
    apps: list[Package],
    skippedApps: dict[Package, str],
    extractArguments: str,
    currentDir: str,
    recursive: bool,
    perAppBuild: bool,
    argumentFileDirectory: str,
    packageConfigurationType: str = PluginSharedValues.TYPE_DEFAULT,
    resume: bool = False,
    identity: ArgumentExtractionState.RunIdentity | None = None,
) -> ArgumentExtraction.ArgumentExtractionReport | None:
    """Build and run the apps '--ExtractArguments' selects and say what happened to each of them.
    - apps are the apps of the READMEs, skippedApps the apps a skipped requirement of the project leaves out (with the name of the requirement).
    - perAppBuild: one build per app instead of one build for all of them.
    - packageConfigurationType is the package configuration the build resolves the gen files of the apps with.
    - identity says which run this is. It is stored with the argument files, so a run that is stopped can be resumed. Without it the run
      leaves nothing a resume can use.
    - resume ('--Resume'): an app that has a valid argument file from an earlier run with the same identity is not run again. Without such
      a run every app is built and run.
    Returns None for a dry run: nothing is built, run or changed, the apps that would be are printed.
    Raises ArgumentExtraction.ConfigureFailedException when the configure of the one build of all apps fails, and
    ArgumentExtractionState.ResumeRefusedException before anything is built when the earlier run has another identity.
    """
    candidates = sorted([*apps, *skippedApps.keys()], key=lambda s: "" if s.AbsolutePath is None else s.AbsolutePath.lower())
    selected = ArgumentExtraction.SelectApps(candidates, extractArguments, currentDir, recursive)

    excluded: dict[Package, ArgumentExtraction.AppExtraction] = {}
    for app in selected:
        if app in skippedApps:
            reason = f"the requirement '{skippedApps[app]}' is skipped"
            excluded[app] = ArgumentExtraction.AppExtraction(app, ArgumentExtraction.ExtractionState.Excluded, reason)
        elif not app.ResolvedPlatformSupported:
            excluded[app] = ArgumentExtraction.AppExtraction(app, ArgumentExtraction.ExtractionState.Excluded, "not supported on this platform")
    appsToBuild = [app for app in selected if app not in excluded]

    # The apps an earlier run extracted the arguments of
    state = ArgumentExtractionState.ArgumentExtractionState(config, argumentFileDirectory)
    extracted: dict[Package, ArgumentExtraction.AppExtraction] = {}
    if resume:
        if identity is None:
            raise Exception("Internal error, a resume needs the identity of the run")
        earlierIdentity = state.TryReadIdentity()
        if earlierIdentity is None:
            config.DoPrint(f"--Resume: no earlier argument extraction to resume was found in '{argumentFileDirectory}', every selected app is built and run")
            resume = False
        else:
            differences = identity.GetDifferences(earlierIdentity)
            if len(differences) > 0:
                raise ArgumentExtractionState.ResumeRefusedException(differences)
            for app in appsToBuild:
                arguments = state.TryReadArguments(app.Name)
                if arguments is not None:
                    extracted[app] = ArgumentExtraction.AppExtraction(app, ArgumentExtraction.ExtractionState.Extracted, "", arguments, True)
    appsToRun = [app for app in appsToBuild if app not in extracted]

    if config.IsDryRun:
        config.DoPrint(f"Would build and run {ArgumentExtraction.FormatAppCount(len(appsToRun))} to extract the command line arguments:")
        for app in appsToRun:
            config.DoPrint(f"- {app.Name}")
        if resume:
            config.DoPrint(f"Would take the arguments of {ArgumentExtraction.FormatAppCount(len(extracted))} from the earlier run:")
            for app in extracted:
                config.DoPrint(f"- {app.Name}")
        return None

    if len(appsToRun) > 0:
        # The file of an app is the proof that its arguments were extracted: an app that is run starts without one
        if resume:
            state.RemoveArguments(app.Name for app in appsToRun)
        else:
            state.Begin(identity)
        if perAppBuild:
            entries = __ExtractWithOneBuildPerApp(toolAppContext, config, appsToRun, argumentFileDirectory, packageConfigurationType)
        else:
            entries = __ExtractWithOneBuild(toolAppContext, config, appsToBuild, appsToRun, argumentFileDirectory, packageConfigurationType)
        for entry in entries:
            extracted[entry.Package] = entry
        # What an app that failed left is not its arguments: a resume runs it again
        state.RemoveArguments(entry.Name for entry in entries if entry.State != ArgumentExtraction.ExtractionState.Extracted)
    return ArgumentExtraction.ArgumentExtractionReport([excluded[app] if app in excluded else extracted[app] for app in selected])


def FinishArgumentExtraction(
    log: Log, report: ArgumentExtraction.ArgumentExtractionReport, state: ArgumentExtractionState.ArgumentExtractionState | None = None
) -> None:
    """The end of a run that extracted arguments, after the READMEs were written: print the summary and fail the run when the arguments of
    an app that was not excluded could not be extracted.
    state: what the run left on disk for a resume. It is removed when there is nothing to resume: no app failed. A run with an app that
    failed keeps it, so a '--Resume' only runs that app again.
    """
    for line in report.FormatSummary(log.Verbosity >= 2):
        log.DoPrint(line)
    if report.HasFailures:
        raise ExitException(ArgumentExtraction.FailureExitCode)
    if state is not None:
        names = [entry.Name for entry in report.Apps if entry.State != ArgumentExtraction.ExtractionState.Excluded]
        if len(names) > 0:
            state.Remove(names)


def GetGenerator(toolAppContext: ToolAppContext, config: Config, toolAppConfig: ToolAppConfig) -> GeneratorPlugin:
    """The generator of the platform the command line asks for"""
    buildVariantConfig = BuildVariantConfigUtil.GetBuildVariantConfig(toolAppConfig.BuildVariantConstraints)
    variableContext = VariableContextHelper.Create(config.ToolConfig, toolAppConfig.UserSetVariables)
    return toolAppContext.PluginConfigContext.GetGeneratorPluginById(
        toolAppConfig.PlatformName,
        toolAppConfig.Generator,
        buildVariantConfig,
        variableContext.UserSetVariables,
        config.ToolConfig.DefaultPackageLanguage,
        config.ToolConfig.CMakeConfiguration,
        toolAppConfig.GetUserCMakeConfig(),
        False,
    )


def ExtractArguments(
    toolAppContext: ToolAppContext, config: Config, exePackages: list[Package], extractArguments: str, currentDir: str
) -> dict[Package, JsonDictType]:
    """The arguments of the apps among exePackages that '--ExtractArguments' selects, of the ones it worked for. All apps are built with
    one build, see RunArgumentExtraction for the report of every app.
    """
    appConfig = toolAppContext.ToolAppConfig
    argumentFileDirectory = GetArgumentFileDirectory(config, GetGenerator(toolAppContext, config, appConfig))
    report = RunArgumentExtraction(toolAppContext, config, exePackages, {}, extractArguments, currentDir, appConfig.Recursive, False, argumentFileDirectory)
    return {} if report is None else report.GetArguments()


def IsNamespaceRootMDFile(lines: list[str]) -> bool:
    return len(lines) > 0 and lines[0].startswith("<!-- #AG_PROJECT_NAMESPACE_ROOT# -->")


def __TryFindRequirementInSet(requirements: list[PackageRequirement], ignoreRequirementSet: set[str]) -> PackageRequirement | None:
    for requirement in requirements:
        if requirement.Name in ignoreRequirementSet:
            return requirement
    return None


def __RemoveIgnored(log: Log, packages: list[Package], ignoreRequirementSet: set[str]) -> list[Package]:
    if len(ignoreRequirementSet) <= 0:
        return packages
    filteredPackages: list[Package] = []
    for package in packages:
        skipRequirement = __TryFindRequirementInSet(package.ResolvedAllUsedFeatures, ignoreRequirementSet)
        if skipRequirement is None:
            filteredPackages.append(package)
        else:
            log.LogPrint(f"Skipping '{package.Name}' because requirement '{skipRequirement.Name}' was set to skip")
    return filteredPackages


def __GetIgnoreRequirementSet(buildDocConfiguration: BuildDocConfiguration) -> set[str]:
    return {requirement.Name for requirement in buildDocConfiguration.Requirements if requirement.Skip}


def GetReadmeApps(packages: list[Package], buildDocConfiguration: BuildDocConfiguration) -> tuple[list[Package], dict[Package, str]]:
    """The apps of the READMEs (the executables that are shown in the main README.md), and the ones among them that a skipped requirement of
    the project leaves out, with the name of that requirement
    """
    ignoreRequirementSet = __GetIgnoreRequirementSet(buildDocConfiguration)
    apps: list[Package] = []
    skippedApps: dict[Package, str] = {}
    for package in ExtractPackages(packages, PackageType.Executable):
        if package.ShowInMainReadme:
            skipRequirement = __TryFindRequirementInSet(package.ResolvedAllUsedFeatures, ignoreRequirementSet)
            if skipRequirement is None:
                apps.append(package)
            else:
                skippedApps[package] = skipRequirement.Name
    return (apps, skippedApps)


class NamespaceReadmeRecord:
    def __init__(self, packageLocationRootDirEx: str, filePath: str, fileContent: list[str], caption: str) -> None:
        super().__init__()
        self.PackageLocationRootDirEx = packageLocationRootDirEx
        self.FilePath = filePath
        self.FileDirectoryEx = IOUtil.GetDirectoryName(filePath) + "/"
        self.FileContent = fileContent
        self.Caption = caption
        self.NewContent: list[str] = []
        self.Keys: set[str] = set()


class NamespaceRecord:
    def __init__(self, namespaceName: str):
        super().__init__()
        self.NamespaceName = namespaceName
        self.PackageList: list[Package] = []

    def AddPackage(self, log: Log, packageDirToReadme: dict[str, NamespaceReadmeRecord | None], package: Package) -> None:
        self.PackageList.append(package)
        self.__PopulatePackageDirToReadme(log, packageDirToReadme, package)

    def __PopulatePackageDirToReadme(self, log: Log, packageDirToReadme: dict[str, NamespaceReadmeRecord | None], package: Package) -> None:
        if package.Path is None:
            raise Exception("internal error")
        packageAbsoluteDirPath = package.Path.AbsoluteDirPath
        packageRootPath = package.Path.PackageRootLocation.ResolvedPathEx

        # If the package is not located at the package root then skip the package dir and start at the parent
        self.__AddClosestNamespaceGroupReadme(log, packageDirToReadme, packageRootPath, packageAbsoluteDirPath, True)

    def __AddClosestNamespaceGroupReadme(
        self,
        log: Log,
        packageDirToReadme: dict[str, NamespaceReadmeRecord | None],
        packageLocationRootDirEx: str,
        currentPackageDir: str,
        skipDirReadmeCheck: bool,
    ) -> NamespaceReadmeRecord | None:
        isRoot = False
        if not currentPackageDir.startswith(packageLocationRootDirEx):
            isRoot = currentPackageDir == packageLocationRootDirEx[0:-1]
            if not isRoot:
                raise Exception(
                    f"usage error, the package dir '{currentPackageDir}' did not start with the package root dir '{packageLocationRootDirEx[0:-1]}'"
                )

        # Check if the package dir has been cached
        if currentPackageDir in packageDirToReadme:
            return packageDirToReadme[currentPackageDir]

        # Allow us to skip reading the README.md file located in the package root.
        # Since it will likely always be present and its not a target for this.
        if not skipDirReadmeCheck:
            readmeFile = IOUtil.Join(currentPackageDir, "README.md")
            fileContent = TryLoadReadMe(log, readmeFile)
            if fileContent is not None and IsNamespaceRootMDFile(fileContent):
                caption = IOUtil.GetFileName(IOUtil.GetDirectoryName(readmeFile))
                readmeRecord = NamespaceReadmeRecord(packageLocationRootDirEx, readmeFile, fileContent, caption)
                packageDirToReadme[currentPackageDir] = readmeRecord
                return readmeRecord

        # Not found yet
        if not isRoot:
            foundFile = self.__AddClosestNamespaceGroupReadme(
                log, packageDirToReadme, packageLocationRootDirEx, IOUtil.GetDirectoryName(currentPackageDir), False
            )
            packageDirToReadme[currentPackageDir] = foundFile
            return foundFile
        return None


# def CheckForNamespaceRootReadMe(activeRootDir: ToolConfigRootDirectory, namespaceDict: Dict[str, List[Package]]):
#     pass


def ProcessPackages(
    toolAppContext: ToolAppContext,
    config: Config,
    packages: list[Package],
    activeRootDir: ToolConfigRootDirectory,
    extractArguments: str | None,
    buildDocConfiguration: BuildDocConfiguration,
    currentDir: str,
    projectRootNamespaceReadmeRecord: NamespaceReadmeRecord,
    packageArgumentsDict: dict[Package, JsonDictType] | None = None,
) -> list[NamespaceReadmeRecord]:
    """packageArgumentsDict: the arguments that were extracted already (FslBuildDoc extracts them once for all projects). Without it the
    arguments are extracted here when extractArguments asks for it.
    """
    log: Log = config
    ignoreRequirementSet = __GetIgnoreRequirementSet(buildDocConfiguration)

    exePackages = ExtractPackages(packages, PackageType.Executable)
    exePackages = __RemoveIgnored(log, exePackages, ignoreRequirementSet)
    exePackages = [package for package in exePackages if package.ShowInMainReadme]
    exePackages.sort(key=lambda s: "" if s.AbsolutePath is None else s.AbsolutePath.lower())

    if packageArgumentsDict is None:
        packageArgumentsDict = {}
        if extractArguments is not None:
            packageArgumentsDict = ExtractArguments(toolAppContext, config, exePackages, extractArguments, currentDir)

    # Run through the exe files and group the exe package into 'namespaces'
    # The dictionary uses the 'namespace' as the key and a list of packages belonging to the namespace as the value
    packageDirToReadme: dict[str, NamespaceReadmeRecord | None] = {}
    namespaceDict: dict[str, NamespaceRecord] = {}
    for package in exePackages:
        if package.NameInfo.Namespace.Value not in namespaceDict:
            namespaceDict[package.NameInfo.Namespace.Value] = NamespaceRecord(package.NameInfo.Namespace.Value)
        namespaceDict[package.NameInfo.Namespace.Value].AddPackage(log, packageDirToReadme, package)

    # Take all found packages for each namespace and sort them based on their package name
    for record in list(namespaceDict.values()):
        record.PackageList.sort(key=lambda s: s.Name.lower())

    # sort the found package namespaces
    sortedKeys = list(namespaceDict.keys())
    sortedKeys.sort()

    # CheckForNamespaceRootReadMe(activeRootDir, namespaceDict)

    # Format it
    foundReadmeRecords = set()
    resultReadmeRecords: list[NamespaceReadmeRecord] = []
    for key in sortedKeys:
        for package in namespaceDict[key].PackageList:
            if package.AbsolutePath is None:
                raise Exception("Invalid package")
            rootDir = config.ToolConfig.TryFindRootDirectory(package.AbsolutePath)
            if rootDir == activeRootDir:
                # locate the namespace readme record
                namespaceReadmeRecord = projectRootNamespaceReadmeRecord
                if package.AbsolutePath in packageDirToReadme:
                    foundNamespaceReadmeRecord = packageDirToReadme[package.AbsolutePath]
                    if foundNamespaceReadmeRecord is not None:
                        namespaceReadmeRecord = foundNamespaceReadmeRecord

                # Append the 'namespace' title
                if key not in namespaceReadmeRecord.Keys:
                    namespaceReadmeRecord.Keys.add(key)
                    if len(namespaceReadmeRecord.NewContent) <= 0:
                        namespaceReadmeRecord.NewContent.append("")
                    namespaceReadmeRecord.NewContent.append(f"## {key}")
                    namespaceReadmeRecord.NewContent.append("")
                    if namespaceReadmeRecord != projectRootNamespaceReadmeRecord:
                        # Add a link in the main file
                        projectRootNamespaceReadmeRecord.NewContent.append(f"## {key}")
                        projectRootNamespaceReadmeRecord.NewContent.append("")
                        linkRelativePath = IOUtil.RelativePath(namespaceReadmeRecord.FileDirectoryEx, projectRootNamespaceReadmeRecord.FileDirectoryEx)
                        linkRelativePath = IOUtil.Join(linkRelativePath, "README.md")
                        projectRootNamespaceReadmeRecord.NewContent.append(f"See [{key}]({linkRelativePath}#{TocEntryLink(key)}) applications")
                        projectRootNamespaceReadmeRecord.NewContent.append("")

                relativeFromPath = namespaceReadmeRecord.FileDirectoryEx
                config.LogPrintVerbose(4, f"Processing package '{package.Name}'")
                packageDir = package.AbsolutePath[len(relativeFromPath) :]
                namespaceReadmeRecord.NewContent.append(f"### [{package.NameInfo.ShortName.Value}]({packageDir})")
                namespaceReadmeRecord.NewContent.append("")
                exampleImagePath = IOUtil.Join(package.AbsolutePath, "Thumbnail.jpg")
                if not IOUtil.IsFile(exampleImagePath):
                    exampleImagePath = IOUtil.Join(package.AbsolutePath, "Example.jpg")
                if IOUtil.IsFile(exampleImagePath):
                    exampleImagePath = exampleImagePath[len(relativeFromPath) :]
                    namespaceReadmeRecord.NewContent.append(
                        f'<a href="{exampleImagePath}"><img src="{exampleImagePath}" height="108px" title="{package.Name}"></a>'
                    )
                    namespaceReadmeRecord.NewContent.append("")

                readmePath = IOUtil.Join(package.AbsolutePath, "README.md")
                if package not in packageArgumentsDict:
                    # The argument section is not written by this run
                    CorrectArgumentHeadingOfOlderVersions(config, readmePath)
                packageReadMeLines = TryLoadReadMe(log, readmePath)
                if packageReadMeLines is not None:
                    packageReadMeLines = UpdatePackageReadMe(config, package, packageReadMeLines, packageArgumentsDict, readmePath)
                    SaveReadMe(config, readmePath, packageReadMeLines)
                    brief = TryExtractBrief(config, packageReadMeLines, readmePath)
                    if brief is not None:
                        namespaceReadmeRecord.NewContent = namespaceReadmeRecord.NewContent + brief
                        namespaceReadmeRecord.NewContent.append("")

                # namespaceReadmeRecord.NewContent.append("")
                if namespaceReadmeRecord.FilePath not in foundReadmeRecords:
                    foundReadmeRecords.add(namespaceReadmeRecord.FilePath)
                    resultReadmeRecords.append(namespaceReadmeRecord)
            # else:
            #    config.LogPrintVerbose(4, "Skipping package '{0}' with rootDir '{1}' is not part of the activeRootDir '{2}'".format(package.Name, rootDir.ResolvedPath, activeRootDir.ResolvedPath))

    return resultReadmeRecords


class DefaultValue:
    DryRun = False
    ToCDepth = 2
    ExtractArguments: str | None = None
    NoMdScan = False
    PerAppBuild = False
    Resume = False


class LocalToolConfig(ToolAppConfig):
    def __init__(self) -> None:
        super().__init__()

        self.DryRun = DefaultValue.DryRun
        self.ToCDepth = DefaultValue.ToCDepth
        self.ExtractArguments = DefaultValue.ExtractArguments
        self.NoMdScan = DefaultValue.NoMdScan
        self.PerAppBuild = DefaultValue.PerAppBuild
        self.Resume = DefaultValue.Resume


def GetDefaultLocalConfig() -> LocalToolConfig:
    return LocalToolConfig()


class ToolFlowBuildDoc(AToolAppFlow):
    # def __init__(self, toolAppContext: ToolAppContext) -> None:
    #    super().__init__(toolAppContext)

    def ProcessFromCommandLine(self, args: Any, currentDirPath: str, toolConfig: ToolConfig, userTag: object | None) -> None:
        # Process the input arguments here, before calling the real work function

        localToolConfig = LocalToolConfig()

        # Configure the ToolAppConfig part
        localToolConfig.SetToolAppConfigValues(self.ToolAppContext.ToolAppConfig)

        # Configure the local part
        localToolConfig.DryRun = args.DryRun
        localToolConfig.ToCDepth = int(args.ToCDepth)
        localToolConfig.ExtractArguments = args.ExtractArguments
        localToolConfig.NoMdScan = args.NoMdScan
        localToolConfig.PerAppBuild = args.PerAppBuild
        localToolConfig.Resume = args.Resume

        self.Process(currentDirPath, toolConfig, localToolConfig)

    def __TryLocateRootDirectory(self, toolRootDirs: list[ToolConfigRootDirectory], findLocation: ResolvedPath) -> ToolConfigRootDirectory | None:
        for rootDir in toolRootDirs:
            if rootDir.ResolvedPath == findLocation.ResolvedPath:
                return rootDir
        return None

    def ProcessSCRFile(self, config: Config, projectContext: ToolConfigProjectContext, scrFilePath: str) -> None:
        templatePath = IOUtil.Join(config.SDKConfigTemplatePath, "Scr")
        filename = IOUtil.GetFileName(scrFilePath)
        fileTemplatePath = IOUtil.Join(templatePath, filename)
        # The template is copied: it is read and written as UTF-8, and bytes that are not valid UTF-8 are kept as they are
        templateFileContent = TextFileReader.TryReadUTF8PassThrough(config, fileTemplatePath, "SCR template")
        if templateFileContent is not None:
            projectVersion = f"{projectContext.ProjectVersion.Major}.{projectContext.ProjectVersion.Minor}.{projectContext.ProjectVersion.Patch}"
            finalContent = templateFileContent
            finalContent = finalContent.replace("##PROJECT_NAME##", projectContext.ProjectName)
            finalContent = finalContent.replace("##PROJECT_VERSION##", projectVersion)

            if not config.IsDryRun:
                IOUtil.WriteFileUTF8(scrFilePath, finalContent, errors="surrogateescape")

    def Process(self, currentDirPath: str, toolConfig: ToolConfig, localToolConfig: LocalToolConfig) -> None:
        if localToolConfig.Resume and localToolConfig.ExtractArguments is None:
            raise UsageErrorException("--Resume can only be used together with --ExtractArguments")

        config = Config(self.Log, toolConfig, "sdk", localToolConfig.BuildVariantConstraints, localToolConfig.AllowDevelopmentPlugins)
        if localToolConfig.DryRun:
            config.ForceDisableAllWrite()
        config.DetectDuplicatePackages = localToolConfig.DetectDuplicatePackages

        if localToolConfig.ToCDepth < 1:
            localToolConfig.ToCDepth = 1
        elif localToolConfig.ToCDepth > 4:
            localToolConfig.ToCDepth = 4

        config.PrintTitle()

        # Get the generator and see if its supported
        variableContext = VariableContextHelper.Create(toolConfig, localToolConfig.UserSetVariables)
        generator = GetGenerator(self.ToolAppContext, config, localToolConfig)
        PlatformUtil.CheckBuildPlatform(generator.PlatformName)

        config.LogPrint(f"Active platform: {generator.PlatformName}")

        packageFilters = localToolConfig.BuildPackageFilters

        minimalConfig = toolConfig.GetMinimalConfig(generator.CMakeConfig)
        # Every package of the project is loaded, so '-r' has no meaning for the scan (it says so). With '--ExtractArguments .' it selects
        # the apps below the current directory instead.
        scanRecursive = localToolConfig.Recursive and localToolConfig.ExtractArguments is None
        theFiles = MainFlow.DoGetFiles(
            config, minimalConfig, currentDirPath, scanRecursive, additionalDirs=self.ToolAppContext.LowLevelToolConfig.AdditionalInputDirs
        )
        generatorContext = GeneratorContext(
            config, self.ErrorHelpManager, packageFilters.RecipeFilterManager, config.ToolConfig.Experimental, generator, variableContext
        )
        packages = MainFlow.DoGetPackages(generatorContext, config, theFiles, packageFilters, engineResolveConfig=EngineResolveConfig.CreateDefaultFlavor())
        # topLevelPackage = PackageListUtil.GetTopLevelPackage(packages)
        # featureList = [entry.Name for entry in topLevelPackage.ResolvedAllUsedFeatures]

        # The arguments are extracted once, for the apps of every project
        extractionReport: ArgumentExtraction.ArgumentExtractionReport | None = None
        extractionState: ArgumentExtractionState.ArgumentExtractionState | None = None
        packageArgumentsDict: dict[Package, JsonDictType] | None = None
        if localToolConfig.ExtractArguments is not None:
            apps, skippedApps = GetReadmeApps(packages, toolConfig.BuildDocConfiguration)
            argumentFileDirectory = GetArgumentFileDirectory(config, generator)
            extractionState = ArgumentExtractionState.ArgumentExtractionState(config, argumentFileDirectory)
            identity = ArgumentExtractionState.CreateRunIdentity(
                toolConfig,
                generator,
                localToolConfig,
                self.ToolAppContext.LowLevelToolConfig.AdditionalInputDirs,
                localToolConfig.ExtractArguments,
                currentDirPath,
                localToolConfig.Recursive,
            )
            extractionReport = RunArgumentExtraction(
                self.ToolAppContext,
                config,
                apps,
                skippedApps,
                localToolConfig.ExtractArguments,
                currentDirPath,
                localToolConfig.Recursive,
                localToolConfig.PerAppBuild,
                argumentFileDirectory,
                resume=localToolConfig.Resume,
                identity=identity,
            )
            packageArgumentsDict = {} if extractionReport is None else extractionReport.GetArguments()

        for projectContext in config.ToolConfig.ProjectInfo.Contexts:
            rootDir = self.__TryLocateRootDirectory(config.ToolConfig.RootDirectories, projectContext.Location)
            if rootDir is None:
                raise Exception(f"Root directory not found for location {projectContext.Location}")

            readmePath = IOUtil.Join(rootDir.ResolvedPath, "README.md")
            packageReadMeLines = TryLoadReadMe(config, readmePath)
            packageReadMeLines = packageReadMeLines if packageReadMeLines is not None else []
            projectRootNamepaceRecord = NamespaceReadmeRecord(rootDir.ResolvedPath, readmePath, packageReadMeLines, "")
            namespaceRecords = ProcessPackages(
                self.ToolAppContext,
                config,
                packages,
                rootDir,
                localToolConfig.ExtractArguments,
                toolConfig.BuildDocConfiguration,
                currentDirPath,
                projectRootNamepaceRecord,
                packageArgumentsDict,
            )

            if projectRootNamepaceRecord not in namespaceRecords:
                namespaceRecords.append(projectRootNamepaceRecord)

            for namespaceRecord in namespaceRecords:
                if len(namespaceRecord.FileContent) > 0:
                    updatedReadMeLines = namespaceRecord.FileContent
                    projectCaptionLines = [f"# {projectContext.ProjectName} {projectContext.ProjectVersion} Unofficial"]
                    if namespaceRecord != projectRootNamepaceRecord:
                        projectCaptionLines[0] = f"{projectCaptionLines[0]} {namespaceRecord.Caption}"
                        relativePathToRoot = IOUtil.RelativePath(rootDir.ResolvedPath, namespaceRecord.FileDirectoryEx)
                        projectCaptionLines.append("")
                        projectCaptionLines.append(f"To [main document]({relativePathToRoot}/README.md)")

                    updatedReadMeLinesNew = TryReplaceSection(config, updatedReadMeLines, "AG_PROJECT_CAPTION", projectCaptionLines, namespaceRecord.FilePath)
                    if updatedReadMeLinesNew is not None:
                        updatedReadMeLines = updatedReadMeLinesNew

                    updatedReadMeLinesNew = TryReplaceSection(config, updatedReadMeLines, "AG_DEMOAPPS", namespaceRecord.NewContent, namespaceRecord.FilePath)
                    if updatedReadMeLinesNew is not None:
                        updatedReadMeLines = updatedReadMeLinesNew

                    # WARNING: The table of content might get regenerated during ProcessMDFiles below
                    tableOfContentDepth = localToolConfig.ToCDepth
                    if namespaceRecord != projectRootNamepaceRecord:
                        tableOfContentDepth = tableOfContentDepth + 1
                    updatedReadMeLinesNew = TryInsertTableOfContents(config, updatedReadMeLines, tableOfContentDepth, namespaceRecord.FilePath)
                    if updatedReadMeLinesNew is not None:
                        updatedReadMeLines = updatedReadMeLinesNew

                    SaveReadMe(config, namespaceRecord.FilePath, updatedReadMeLines)
                elif config.Verbosity > 2:
                    config.LogPrintWarning(f"No README.md found at {namespaceRecord.FilePath}")

            if not localToolConfig.NoMdScan:
                mdFiles = IOUtil.FindFileByExtension(rootDir.ResolvedPath, ".md", minimalConfig.IgnoreDirectories)
                for filename in mdFiles:
                    if filename != readmePath:
                        config.LogPrintVerbose(1, f"Processing file '{filename}'")
                        self.ProcessMDFile(config, filename, localToolConfig)

            scrFilesCandidates = IOUtil.GetFilesAt(rootDir.ResolvedPath, True)
            for scrFilePath in scrFilesCandidates:
                if scrFilePath.endswith(".txt") and IOUtil.GetFileName(scrFilePath).startswith("SCR-"):
                    self.ProcessSCRFile(config, projectContext, scrFilePath)

        if extractionReport is not None:
            FinishArgumentExtraction(config, extractionReport, extractionState)

    def ProcessMDFile(self, config: Config, filename: str, localToolConfig: LocalToolConfig) -> None:
        # Also a README.md that is not the one of a package can have an argument section an older version wrote
        CorrectArgumentHeadingOfOlderVersions(config, filename)
        packageReadMeLines = TryLoadReadMe(config, filename)
        if packageReadMeLines is not None and self.HasLineThatContain(packageReadMeLines, "#AG_TOC_BEGIN#") and not IsNamespaceRootMDFile(packageReadMeLines):
            packageReadMeLinesNew = TryInsertTableOfContents(config, packageReadMeLines, localToolConfig.ToCDepth, filename)
            if packageReadMeLinesNew is not None:
                packageReadMeLines = packageReadMeLinesNew

            SaveReadMe(config, filename, packageReadMeLines)

    def HasLineThatContain(self, lines: list[str], strFind: str) -> bool:
        return any(strFind in entry for entry in lines)


class ToolAppFlowFactory(AToolAppFlowFactory):
    # def __init__(self) -> None:
    #    pass

    def GetTitle(self) -> str:
        return "FslBuildDoc"

    def GetToolCommonArgConfig(self) -> ToolCommonArgConfig:
        argConfig = ToolCommonArgConfig()
        argConfig.AddPlatformArg = True
        argConfig.AddGeneratorSelection = True
        argConfig.SupportBuildTime = True
        # argConfig.AllowVSVersion = True
        # These are used when: --ExtractArguments is enabled
        argConfig.AddBuildFiltering = True
        argConfig.AddBuildThreads = True
        argConfig.AddBuildVariants = True
        argConfig.AllowRecursive = True
        argConfig.AllowDetectDuplicatePackages = True
        return argConfig

    def AddCustomArguments(self, parser: argparse.ArgumentParser, toolConfig: ToolConfig, userTag: object | None) -> None:
        parser.add_argument("--DryRun", action="store_true", help="No files will be created")
        parser.add_argument("--ToCDepth", default=str(DefaultValue.ToCDepth), help=f"The headline depth to include, defaults to {DefaultValue.ToCDepth} (1-4)")
        parser.add_argument(
            "--ExtractArguments",
            default=DefaultValue.ExtractArguments,
            help='Build the apps and execute them to extract the command line arguments ("*" for all, "." for the package of the current directory and with -r every package below it). All apps are built with one build. A summary says which apps it did not work for and the exit code is non-zero then. An app the feature filters (--UseFeatures) remove is not built and not in the summary.',
        )
        parser.add_argument(
            "--PerAppBuild",
            action="store_true",
            help="With --ExtractArguments: one build per app instead of one build for all of them, so an app that can not be configured only costs itself.",
        )
        parser.add_argument(
            "--Resume",
            action="store_true",
            help="With --ExtractArguments: carry on with an extraction that was stopped, or that ended with apps that failed. An app the earlier run extracted the arguments of is not run again, the other ones are built and run. It is the same run a little later: the platform, the generator, the variants, the feature filters, the selected apps and the tool version have to be the ones of the earlier run, otherwise it is refused and says what differs. A gen file or a source file that changed since is not noticed. A run without --Resume starts fresh.",
        )
        parser.add_argument("--NoMdScan", action="store_true", help='Dont scan for all "md" files and generate a table of content')

    def Create(self, toolAppContext: ToolAppContext) -> AToolAppFlow:
        return ToolFlowBuildDoc(toolAppContext)
