#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#****************************************************************************************************************************************************
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
#****************************************************************************************************************************************************

from re import S
from tkinter import CURRENT
from typing import Any
#from typing import Callable
from typing import cast
from typing import Dict
from typing import Optional
from typing import List
from typing import Set
import argparse
import json
from FslBuildGen import IOUtil
from FslBuildGen import Main as MainFlow
from FslBuildGen.BasicConfig import BasicConfig
from FslBuildGen.Build.BuildVariantConfigUtil import BuildVariantConfigUtil
from FslBuildGen.BuildConfig.BuildDocConfiguration import BuildDocConfiguration
from FslBuildGen.Config import Config
from FslBuildGen.Build.ForAllConfig import ForAllConfig
from FslBuildGen.Context.GeneratorContext import GeneratorContext
from FslBuildGen.DataTypes import PackageType
from FslBuildGen.Engine.EngineResolveConfig import EngineResolveConfig
from FslBuildGen.Location.ResolvedPath import ResolvedPath
from FslBuildGen.Log import Log
from FslBuildGen.Packages.Package import Package
from FslBuildGen.Packages.PackageRequirement import PackageRequirement
from FslBuildGen.PlatformUtil import PlatformUtil
from FslBuildGen.Tool.AToolAppFlow import AToolAppFlow
from FslBuildGen.Tool.AToolAppFlowFactory import AToolAppFlowFactory
from FslBuildGen.Tool.ToolAppConfig import ToolAppConfig
from FslBuildGen.Tool.ToolAppContext import ToolAppContext
from FslBuildGen.Tool.ToolCommonArgConfig import ToolCommonArgConfig
from FslBuildGen.ToolConfig import ToolConfig
from FslBuildGen.ToolConfigRootDirectory import ToolConfigRootDirectory
from FslBuildGen.Tool.Flow import ToolFlowBuild
from FslBuildGen.ToolConfigProjectContext import ToolConfigProjectContext
from FslBuildGen.VariableContextHelper import VariableContextHelper


JsonDictType = Dict[str, Any]


def ExtractPackages(packages: List[Package], packageType: PackageType) -> List[Package]:
    res = []  # type: List[Package]
    for package in packages:
        if package.Type == packageType:
            res.append(package)
    return res


def IndexOf(lines: List[str], magicCommentContent: str) -> int:
    actualSearchString = "<!-- #{0}# -->".format(magicCommentContent)
    for idx, val in enumerate(lines):
        if val.strip() == actualSearchString:
            return idx
    return -1


def RightStripLines(lines: List[str]) -> None:
    for index, entry in enumerate(lines):
        lines[index] = entry.rstrip()


def TryReplaceSection(basicConfig: BasicConfig,
                      lines: List[str],
                      sectionName: str,
                      replacementLines: List[str],
                      path: str) -> Optional[List[str]]:
    sectionBegin = "{0}_BEGIN".format(sectionName)
    sectionEnd = "{0}_END".format(sectionName)
    startIndex = IndexOf(lines, sectionBegin)
    if startIndex < 0:
        basicConfig.LogPrint("WARNING: {0} did not contain a {1} section".format(path, sectionBegin))
        return None
    endIndex = IndexOf(lines, sectionEnd)
    if endIndex < 0:
        basicConfig.LogPrint("WARNING: {0} did not contain a {1} section".format(path, sectionEnd))
        return None
    if startIndex >= endIndex:
        basicConfig.LogPrint("WARNING: {0} {1} must appear before {2}".format(path, sectionBegin, sectionEnd))
        return None

    start = lines[0:startIndex+1]
    end = lines[endIndex:]
    return start + replacementLines + end


def TocEntryName(line: str) -> str:
    return line.strip()


def TocEntryLink(line: str) -> str:
    line = line.strip()
    line = line.replace(' ', '-')
    line = line.replace('.', '')
    line = line.replace(':', '')
    line = line.lower()
    return line


def TryTocPrepareLine(line: str) -> Optional[str]:
    line = line.strip()
    if line.startswith('['):
        index = line.find(']')
        if index == -1:
            return None
        return line[1:index]
    return line


def BuildTableOfContents(lines: List[str], depth: int) -> List[str]:
    # Dumb but simple TOC genration
    startIndex = IndexOf(lines, 'AG_TOC_END')
    if startIndex < 0:
        startIndex = 0

    tocLines = []
    for index in range(startIndex, len(lines)):
        line = lines[index]
        if line.startswith("# "):
            line = line[2:]
            resLine = TryTocPrepareLine(line)
            if resLine is not None:
                tocLines.append('* [{0}](#{1})'.format(TocEntryName(resLine), TocEntryLink(resLine)))
        elif line.startswith("## ") and depth >= 2:
            line = line[3:]
            resLine = TryTocPrepareLine(line)
            if resLine is not None:
                tocLines.append('  * [{0}](#{1})'.format(TocEntryName(resLine), TocEntryLink(resLine)))
        elif line.startswith("### ") and depth >= 3:
            line = line[4:]
            resLine = TryTocPrepareLine(line)
            if resLine is not None:
                tocLines.append('    * [{0}](#{1})'.format(TocEntryName(resLine), TocEntryLink(resLine)))
        elif line.startswith("#### ") and depth >= 4:
            line = line[5:]
            resLine = TryTocPrepareLine(line)
            if resLine is not None:
                tocLines.append('      * [{0}](#{1})'.format(TocEntryName(resLine), TocEntryLink(resLine)))
    return tocLines


def TryInsertTableOfContents(basicConfig: BasicConfig, lines: List[str], depth: int, path: str) -> Optional[List[str]]:
    tocLines = BuildTableOfContents(lines, depth)
    return TryReplaceSection(basicConfig, lines, "AG_TOC", tocLines, path)


# AG_DEMOAPP_HEADER_BEGIN
def BuildDemoAppHeader(package: Package) -> List[str]:
    result = []  # type: List[str]
    result.append("# {0}".format(package.NameInfo.ShortName.Value))
#    result.append('<img src="./Example.jpg" height="135px" style="float:right">')
    result.append('<img src="Example.jpg" height="135px">')
    result.append("")
    return result


def TryLoadTextFileAsLines(path: str) -> Optional[List[str]]:
    result = IOUtil.TryReadFile(path)
    if result is None:
        return None
    lines = result.split('\n')
    return lines


def TryLoadReadMe(path: str) -> Optional[List[str]]:
    lines = TryLoadTextFileAsLines(path)
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


class ProgramArgument(object):
    def __init__(self, basicConfig: BasicConfig, entry: Dict[str, Any]) -> None:
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
            self.SourceName = self.SourceName[0:-len(strEnding)]

    def __filterNonPrintable(self, strSrc: str) -> str:
        return ''.join([c for c in strSrc if ord(c) > 31 or ord(c) == 9])


    def __ReadEntry(self, srcDict: Dict[str, Any], name: str, defaultValue: Optional[str] = None) -> str:
        if name in srcDict:
            return self.__filterNonPrintable(srcDict[name].strip())
        elif defaultValue is not None:
            return defaultValue
        raise Exception("command line argument entry missing attribute: '{0}'".format(name))


    def __TryReadEntry(self, srcDict: Dict[Any, str], name: str, defaultValue: Optional[str] = None) -> Optional[str]:
        if name in srcDict:
            return self.__filterNonPrintable(srcDict[name].strip())
        return defaultValue


    def __ReadOptionArgument(self, srcDict: Dict[str, Any], name: str) -> int:
        result = self.__ReadEntry(srcDict, name)
        if result == '0':
            return OptionArgument.OptionNone
        elif result == '1':
            return OptionArgument.OptionRequired
        else:
            raise Exception("command line argument OptionArgument '{0}' has unsupported value of '{1}'".format(name, result))


    def __ReadOptionGroup(self, srcDict: Dict[str, Any], name: str) -> int:
        result = self.__ReadEntry(srcDict, name)
        return int(result)



def DecodeJsonArgumentList(basicConfig: BasicConfig, arguments: List[Dict[str, Any]]) -> List[ProgramArgument]:
    res = []  # type: List[ProgramArgument]
    for entry in arguments:
        res.append(ProgramArgument(basicConfig, entry))
    return res


def GetMaxFormattedNameLength(entries: List[ProgramArgument]) -> int:
    count = 0
    for entry in entries:
        if len(entry.Help_FormattedName) > count:
            count = len(SafeMarkdownString(entry.Help_FormattedName))
    return count


def GetMaxDescriptionLength(entries: List[ProgramArgument]) -> int:
    count = 0
    for entry in entries:
        if len(entry.Description) > count:
            count = len(SafeMarkdownString(entry.Description))
    return count


def GetMaxSourceNameLength(entries: List[ProgramArgument]) -> int:
    count = 0
    for entry in entries:
        if len(entry.SourceName) > count:
            count = len(SafeMarkdownString(entry.Description))
    return count


def SafeMarkdownString(strSrc: str) -> str:
    return strSrc.replace('<', '\\<').replace('|', '\\|')


#def GroupArgumentsBySourceName(basicConfig, arguments):
#    dict = {}
#    for entry in arguments:
#        if not entry.SourceName in dict:
#            dict[entry.SourceName] = []
#        dict[entry.SourceName].append(entry)

#    for entry in dict.values():
#        entry.sort(key=lambda s: s.Help_FormattedName.lower())
#    return dict


def TryBuildArgumentTableLines(basicConfig: BasicConfig, argumentDict: JsonDictType) -> Optional[List[str]]:
    mainKey = "arguments"
    if not mainKey in argumentDict:
        return None

    srcArguments = argumentDict[mainKey] # type: List[Dict[str, Any]]
    arguments = DecodeJsonArgumentList(basicConfig, srcArguments)  # type: List[ProgramArgument]

    maxNameLength = GetMaxFormattedNameLength(arguments)
    maxDescLength = GetMaxDescriptionLength(arguments)
    maxSourceNameLength = GetMaxSourceNameLength(arguments)

    arguments.sort(key=lambda s: (s.SourceName, s.Help_FormattedName.lower()))

    #groupedArgumentDict = GroupArgumentsBySourceName(basicConfig, arguments)

    #sortedKeys = list(arguments.keys())
    #sortedKeys.sort()

    #Command line arguments:
    #
    #Argument            |Description
    #--------------------|---

    formatString = "{{0:<{0}}}|{{1:<{1}}}|{{2}}".format(maxNameLength, maxDescLength)

    result = []  # type: List[str]

#    for key, argumentList in groupedArgumentDict.items():
    result.append("")
    result.append("Command line arguments':")
    result.append("")
    result.append(formatString.format("Argument", "Description", "Source"))
    result.append("{0}|{1}|{2}".format("-"*maxNameLength, "-"*maxDescLength, "-"*maxSourceNameLength))
    for entry in arguments:
        if entry.Group != OptionGroup.Hidden:
            result.append(formatString.format(SafeMarkdownString(entry.Help_FormattedName), SafeMarkdownString(entry.Description), SafeMarkdownString(entry.SourceName)))
    return result



def UpdatePackageReadMe(basicConfig: BasicConfig,
                        package: Package,
                        lines: List[str],
                        packageArgumentsDict: Dict[Package, JsonDictType],
                        path: str) -> List[str]:
    newHeader = BuildDemoAppHeader(package)
    lines2 = TryReplaceSection(basicConfig, lines, "AG_DEMOAPP_HEADER", newHeader, path)
    if lines2 is not None:
        lines = lines2
    if package in packageArgumentsDict:
        argumentsLines = TryBuildArgumentTableLines(basicConfig, packageArgumentsDict[package])
        if argumentsLines is not None:
            newLines = TryReplaceSection(basicConfig, lines, "AG_DEMOAPP_COMMANDLINE_ARGUMENTS", argumentsLines, path)
            if newLines is not None:
                lines = newLines
    return lines


def SaveReadMe(config: Config, path: str, lines: List[str]) -> None:
    text = "\n".join(lines)
    if not config.DisableWrite:
        IOUtil.WriteFileIfChanged(path, text)


def TryExtractBrief(basicConfig: BasicConfig, lines: List[str], path: str) -> Optional[List[str]]:
    startIndex = IndexOf(lines, "AG_BRIEF_BEGIN")
    if startIndex < 0:
        basicConfig.LogPrint("WARNING: {0} did not contain a AG_BRIEF_BEGIN section".format(path))
        return None
    endIndex = IndexOf(lines, "AG_BRIEF_END")
    if endIndex < 0:
        basicConfig.LogPrint("WARNING: {0} did not contain a AG_BRIEF_END section".format(path))
        return None
    if startIndex >= endIndex:
        basicConfig.LogPrint("WARNING: {0} AG_BRIEF_BEGIN must appear before AG_BRIEF_END".format(path))
        return None

    result = lines[startIndex+1:endIndex]

    # remove all empty lines at the end
    index = len(result)-1
    while index >= 0 and len(result[index]) == 0:
        result.pop(index)
        index = index - 1

    return result


def ReadJsonFile(filename: str) -> JsonDictType:
    content = IOUtil.ReadFile(filename)
    return cast(JsonDictType, json.loads(content))


def TryBuildAndRun(toolAppContext: ToolAppContext, config: Config, package: Package) -> Optional[JsonDictType]:
    if not package.ResolvedPlatformSupported:
        return None
    if package.AbsolutePath is None:
        raise Exception("Invalid package")
    workDir = package.AbsolutePath
    tmpOutputFilename = IOUtil.Join(workDir, 'FslBuildDoc_AppArguments.json')
    try:
        # FslBuild.py --ForAllExe "(EXE) --System.Arguments.Save <filename>"

        toolFlowConfig = ToolFlowBuild.GetDefaultLocalConfig()
        toolFlowConfig.SetToolAppConfigValues(toolAppContext.ToolAppConfig)
        toolFlowConfig.ForAllConfig = ForAllConfig.CreateForAllExeConfig('(EXE) --System.Arguments.Save {0} -h'.format(tmpOutputFilename))
        buildFlow = ToolFlowBuild.ToolFlowBuild(toolAppContext)
        buildFlow.Process(workDir, config.ToolConfig, toolFlowConfig)

        return ReadJsonFile(tmpOutputFilename)
    except (Exception) as ex:
        if toolAppContext.LowLevelToolConfig.DebugEnabled:
            raise
        config.LogPrint("Failed to build and run '{0}' due to exception {1}".format(package.Name, ex))
        return None
    finally:
        IOUtil.RemoveFile(tmpOutputFilename)


def ExtractArguments(toolAppContext: ToolAppContext, config: Config, exePackages: List[Package], extractArguments: str, currentDir: str) -> Dict[Package, JsonDictType]:
    config.LogPrint("Building all executable packages to extract their command line arguments")

    filterDir = None if extractArguments == '*' else currentDir

    res = {}  # type: Dict[Package, JsonDictType]
    for package in exePackages:
        if filterDir is None or package.AbsolutePath == filterDir:
            config.LogPrint("- Building and running {0}".format(package.Name))
            arguments = TryBuildAndRun(toolAppContext, config, package)
            if arguments is not None:
                res[package] = arguments

        # quick exit
        #return res
    return res

def IsNamespaceRootMDFile(lines: List[str]) -> bool:
    return len(lines) > 0 and lines[0].startswith("<!-- #AG_PROJECT_NAMESPACE_ROOT# -->")

def __TryFindRequirementInSet(requirements: List[PackageRequirement], ignoreRequirementSet: Set[str]) -> Optional[PackageRequirement]:
    for requirement in requirements:
        if requirement.Name in ignoreRequirementSet:
            return requirement
    return None

def __RemoveIgnored(log: Log, packages: List[Package], ignoreRequirementSet: Set[str]) -> List[Package]:
    if len(ignoreRequirementSet) <= 0:
        return packages
    filteredPackages = [] # type: List[Package]
    for package in packages:
        skipRequirement = __TryFindRequirementInSet(package.ResolvedAllUsedFeatures, ignoreRequirementSet)
        if skipRequirement is None:
            filteredPackages.append(package)
        else:
            log.LogPrint("Skipping '{0}' because requirement '{1}' was set to skip".format(package.Name, skipRequirement.Name))
    return filteredPackages

class NamespaceReadmeRecord(object):
    def __init__(self, packageLocationRootDirEx: str, filePath: str, fileContent: List[str], caption: str) -> None:
        super().__init__()
        self.PackageLocationRootDirEx = packageLocationRootDirEx
        self.FilePath = filePath
        self.FileDirectoryEx = IOUtil.GetDirectoryName(filePath) + '/'
        self.FileContent = fileContent
        self.Caption = caption
        self.NewContent = [] # type: List[str]
        self.Keys = set() # type: Set[str]

class NamespaceRecord(object):
    def __init__(self, namespaceName: str):
        super().__init__()
        self.NamespaceName = namespaceName
        self.PackageList = [] # type: List[Package]

    def AddPackage(self, packageDirToReadme: Dict[str, Optional[NamespaceReadmeRecord]], package: Package) -> None:
        self.PackageList.append(package)
        self.__PopulatePackageDirToReadme(packageDirToReadme, package)

    def __PopulatePackageDirToReadme(self, packageDirToReadme: Dict[str, Optional[NamespaceReadmeRecord]], package: Package) -> None:
        if  package.Path is None:
          raise Exception("internal error")
        packageAbsoluteDirPath = package.Path.AbsoluteDirPath
        packageRootPath = package.Path.PackageRootLocation.ResolvedPathEx

        # If the package is not located at the package root then skip the package dir and start at the parent
        self.__AddClosestNamespaceGroupReadme(packageDirToReadme, packageRootPath, packageAbsoluteDirPath, True)

    def __AddClosestNamespaceGroupReadme(self, packageDirToReadme: Dict[str, Optional[NamespaceReadmeRecord]], packageLocationRootDirEx: str,
                                         currentPackageDir: str, skipDirReadmeCheck: bool) -> Optional[NamespaceReadmeRecord]:
        isRoot = False
        if not currentPackageDir.startswith(packageLocationRootDirEx):
            isRoot = currentPackageDir == packageLocationRootDirEx[0:-1]
            if not isRoot:
                raise Exception("usage error, the package dir '{0}' did not start with the package root dir '{1}'".format(currentPackageDir, packageLocationRootDirEx[0:-1]))

        # Check if the package dir has been cached
        if currentPackageDir in packageDirToReadme:
            return packageDirToReadme[currentPackageDir]

        # Allow us to skip reading the README.md file located in the package root.
        # Since it will likely always be present and its not a target for this.
        if not skipDirReadmeCheck:
            readmeFile = IOUtil.Join(currentPackageDir, "README.md")
            fileContent = TryLoadReadMe(readmeFile)
            if fileContent is not None:
                if IsNamespaceRootMDFile(fileContent):
                    caption = IOUtil.GetFileName(IOUtil.GetDirectoryName(readmeFile))
                    readmeRecord = NamespaceReadmeRecord(packageLocationRootDirEx, readmeFile, fileContent, caption)
                    packageDirToReadme[currentPackageDir] = readmeRecord
                    return readmeRecord

        # Not found yet
        if not isRoot:
            foundFile = self.__AddClosestNamespaceGroupReadme(packageDirToReadme, packageLocationRootDirEx, IOUtil.GetDirectoryName(currentPackageDir), False)
            packageDirToReadme[currentPackageDir] = foundFile
            return foundFile
        return None


# def CheckForNamespaceRootReadMe(activeRootDir: ToolConfigRootDirectory, namespaceDict: Dict[str, List[Package]]):
#     pass

def ProcessPackages(toolAppContext: ToolAppContext, config: Config, packages: List[Package], activeRootDir: ToolConfigRootDirectory,
                    extractArguments: Optional[str], buildDocConfiguration: BuildDocConfiguration, currentDir: str,
                    projectRootNamespaceReadmeRecord: NamespaceReadmeRecord) -> List[NamespaceReadmeRecord]:
    log = config # type: Log
    ignoreRequirementSet = set() # type: Set[str]
    for requirement in buildDocConfiguration.Requirements:
        if requirement.Skip:
            ignoreRequirementSet.add(requirement.Name)

    exePackages = ExtractPackages(packages, PackageType.Executable)
    exePackages = __RemoveIgnored(log, exePackages, ignoreRequirementSet)
    exePackages = [package for package in exePackages if package.ShowInMainReadme]
    exePackages.sort(key=lambda s: "" if s.AbsolutePath is None else s.AbsolutePath.lower())

    packageArgumentsDict = {}  # type: Dict[Package, JsonDictType]
    if extractArguments is not None:
        packageArgumentsDict = ExtractArguments(toolAppContext, config, exePackages, extractArguments, currentDir)

    # Run through the exe files and group the exe package into 'namespaces'
    # The dictionary uses the 'namespace' as the key and a list of packages belonging to the namespace as the value
    packageDirToReadme = dict() # type: Dict[str, Optional[NamespaceReadmeRecord]]
    namespaceDict = {}  # type: Dict[str, NamespaceRecord]
    for package in exePackages:
        if not package.NameInfo.Namespace.Value in namespaceDict:
            namespaceDict[package.NameInfo.Namespace.Value] = NamespaceRecord(package.NameInfo.Namespace.Value)
        namespaceDict[package.NameInfo.Namespace.Value].AddPackage(packageDirToReadme, package)

    # Take all found packages for each namespace and sort them based on their package name
    for record in list(namespaceDict.values()):
        record.PackageList.sort(key=lambda s: s.Name.lower())

    # sort the found package namespaces
    sortedKeys = list(namespaceDict.keys())
    sortedKeys.sort()

    #CheckForNamespaceRootReadMe(activeRootDir, namespaceDict)

    # Format it
    foundReadmeRecords = set()
    resultReadmeRecords = [] # type: List[NamespaceReadmeRecord]
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
                    namespaceReadmeRecord.NewContent.append("## {0}".format(key))
                    namespaceReadmeRecord.NewContent.append("")
                    if namespaceReadmeRecord != projectRootNamespaceReadmeRecord:
                        # Add a link in the main file
                        projectRootNamespaceReadmeRecord.NewContent.append("## {0}".format(key))
                        projectRootNamespaceReadmeRecord.NewContent.append("")
                        linkRelativePath = IOUtil.RelativePath(namespaceReadmeRecord.FileDirectoryEx, projectRootNamespaceReadmeRecord.FileDirectoryEx)
                        linkRelativePath = IOUtil.Join(linkRelativePath, "README.md")
                        projectRootNamespaceReadmeRecord.NewContent.append("See [{0}]({1}#{2}) applications".format(key, linkRelativePath, TocEntryLink(key)))
                        projectRootNamespaceReadmeRecord.NewContent.append("")

                relativeFromPath = namespaceReadmeRecord.FileDirectoryEx
                config.LogPrintVerbose(4, "Processing package '{0}'".format(package.Name))
                packageDir = package.AbsolutePath[len(relativeFromPath):]
                namespaceReadmeRecord.NewContent.append("### [{0}]({1})".format(package.NameInfo.ShortName.Value, packageDir))
                namespaceReadmeRecord.NewContent.append("")
                exampleImagePath = IOUtil.Join(package.AbsolutePath, "Thumbnail.jpg")
                if not IOUtil.IsFile(exampleImagePath):
                    exampleImagePath = IOUtil.Join(package.AbsolutePath, "Example.jpg")
                if IOUtil.IsFile(exampleImagePath):
                    exampleImagePath = exampleImagePath[len(relativeFromPath):]
                    namespaceReadmeRecord.NewContent.append('<a href="{0}"><img src="{0}" height="108px" title="{1}"></a>'.format(exampleImagePath, package.Name))
                    namespaceReadmeRecord.NewContent.append("")

                readmePath = IOUtil.Join(package.AbsolutePath, "README.md")
                packageReadMeLines = TryLoadReadMe(readmePath)
                if packageReadMeLines is not None:
                    packageReadMeLines = UpdatePackageReadMe(config, package, packageReadMeLines, packageArgumentsDict, readmePath)
                    SaveReadMe(config, readmePath, packageReadMeLines)
                    brief = TryExtractBrief(config, packageReadMeLines, readmePath)
                    if brief is not None:
                        namespaceReadmeRecord.NewContent = namespaceReadmeRecord.NewContent + brief
                        namespaceReadmeRecord.NewContent.append("")

                #namespaceReadmeRecord.NewContent.append("")
                if namespaceReadmeRecord.FilePath not in foundReadmeRecords:
                    foundReadmeRecords.add(namespaceReadmeRecord.FilePath)
                    resultReadmeRecords.append(namespaceReadmeRecord)
            #else:
            #    config.LogPrintVerbose(4, "Skipping package '{0}' with rootDir '{1}' is not part of the activeRootDir '{2}'".format(package.Name, rootDir.ResolvedPath, activeRootDir.ResolvedPath))

    return resultReadmeRecords

class DefaultValue(object):
    DryRun = False
    ToCDepth = 2
    ExtractArguments = None # type: Optional[str]
    NoMdScan = False


class LocalToolConfig(ToolAppConfig):
    def __init__(self) -> None:
        super().__init__()

        self.DryRun = DefaultValue.DryRun
        self.ToCDepth = DefaultValue.ToCDepth
        self.ExtractArguments = DefaultValue.ExtractArguments
        self.NoMdScan = DefaultValue.NoMdScan


def GetDefaultLocalConfig() -> LocalToolConfig:
    return LocalToolConfig()


class ToolFlowBuildDoc(AToolAppFlow):
    #def __init__(self, toolAppContext: ToolAppContext) -> None:
    #    super().__init__(toolAppContext)


    def ProcessFromCommandLine(self, args: Any, currentDirPath: str, toolConfig: ToolConfig, userTag: Optional[object]) -> None:
        # Process the input arguments here, before calling the real work function

        localToolConfig = LocalToolConfig()

        # Configure the ToolAppConfig part
        localToolConfig.SetToolAppConfigValues(self.ToolAppContext.ToolAppConfig)

        # Configure the local part
        localToolConfig.DryRun = args.DryRun
        localToolConfig.ToCDepth = int(args.ToCDepth)
        localToolConfig.ExtractArguments = args.ExtractArguments
        localToolConfig.NoMdScan = args.NoMdScan

        self.Process(currentDirPath, toolConfig, localToolConfig)

    def __TryLocateRootDirectory(self, toolRootDirs: List[ToolConfigRootDirectory], findLocation: ResolvedPath) -> Optional[ToolConfigRootDirectory]:
        for rootDir in toolRootDirs:
            if rootDir.ResolvedPath == findLocation.ResolvedPath:
                return rootDir
        return None

    def ProcessSCRFile(self, config: Config, projectContext: ToolConfigProjectContext, scrFilePath: str) -> None:
        templatePath = IOUtil.Join(config.SDKConfigTemplatePath, "Scr")
        filename = IOUtil.GetFileName(scrFilePath)
        fileTemplatePath = IOUtil.Join(templatePath, filename)
        templateFileContent = IOUtil.TryReadFile(fileTemplatePath)
        if templateFileContent is not None:
            projectVersion = "{0}.{1}.{2}".format(projectContext.ProjectVersion.Major, projectContext.ProjectVersion.Minor, projectContext.ProjectVersion.Patch)
            finalContent = templateFileContent
            finalContent = finalContent.replace("##PROJECT_NAME##", projectContext.ProjectName)
            finalContent = finalContent.replace("##PROJECT_VERSION##", projectVersion)

            if not config.IsDryRun:
                IOUtil.WriteFileUTF8(scrFilePath, finalContent)

    def Process(self, currentDirPath: str, toolConfig: ToolConfig, localToolConfig: LocalToolConfig) -> None:
        config = Config(self.Log, toolConfig, 'sdk', localToolConfig.BuildVariantConstraints, localToolConfig.AllowDevelopmentPlugins)
        if localToolConfig.DryRun:
            config.ForceDisableAllWrite()

        if localToolConfig.ToCDepth < 1:
            localToolConfig.ToCDepth = 1
        elif localToolConfig.ToCDepth > 4:
            localToolConfig.ToCDepth = 4

        config.PrintTitle()

        # Get the generator and see if its supported
        buildVariantConfig = BuildVariantConfigUtil.GetBuildVariantConfig(localToolConfig.BuildVariantConstraints)
        variableContext = VariableContextHelper.Create(toolConfig, localToolConfig.UserSetVariables)
        generator = self.ToolAppContext.PluginConfigContext.GetGeneratorPluginById(localToolConfig.PlatformName, localToolConfig.Generator,
                                                                                   buildVariantConfig, variableContext.UserSetVariables,
                                                                                   config.ToolConfig.DefaultPackageLanguage,
                                                                                   config.ToolConfig.CMakeConfiguration,
                                                                                   localToolConfig.GetUserCMakeConfig(), False)
        PlatformUtil.CheckBuildPlatform(generator.PlatformName)

        config.LogPrint("Active platform: {0}".format(generator.PlatformName))

        packageFilters = localToolConfig.BuildPackageFilters

        minimalConfig = toolConfig.GetMinimalConfig(generator.CMakeConfig)
        theFiles = MainFlow.DoGetFiles(config, minimalConfig, currentDirPath, localToolConfig.Recursive)
        generatorContext = GeneratorContext(config, self.ErrorHelpManager, packageFilters.RecipeFilterManager, config.ToolConfig.Experimental,
                                            generator, variableContext)
        packages = MainFlow.DoGetPackages(generatorContext, config, theFiles, packageFilters,
                                          engineResolveConfig = EngineResolveConfig.CreateDefaultFlavor())
        #topLevelPackage = PackageListUtil.GetTopLevelPackage(packages)
        #featureList = [entry.Name for entry in topLevelPackage.ResolvedAllUsedFeatures]

        for projectContext in config.ToolConfig.ProjectInfo.Contexts:
            rootDir = self.__TryLocateRootDirectory(config.ToolConfig.RootDirectories, projectContext.Location)
            if rootDir is None:
                raise Exception("Root directory not found for location {0}".format(projectContext.Location))

            readmePath = IOUtil.Join(rootDir.ResolvedPath, "README.md")
            packageReadMeLines = TryLoadReadMe(readmePath)
            packageReadMeLines = packageReadMeLines if packageReadMeLines is not None else []
            projectRootNamepaceRecord = NamespaceReadmeRecord(rootDir.ResolvedPath, readmePath, packageReadMeLines, "")
            namespaceRecords = ProcessPackages(self.ToolAppContext, config, packages, rootDir, localToolConfig.ExtractArguments,
                                               toolConfig.BuildDocConfiguration, currentDirPath,
                                               projectRootNamepaceRecord)

            if projectRootNamepaceRecord not in namespaceRecords:
                namespaceRecords.append(projectRootNamepaceRecord)

            for namespaceRecord in namespaceRecords:
                if len(namespaceRecord.FileContent) > 0:
                    updatedReadMeLines = namespaceRecord.FileContent
                    projectCaptionLines = ["# {0} {1}".format(projectContext.ProjectName, projectContext.ProjectVersion)]
                    if namespaceRecord != projectRootNamepaceRecord:
                        projectCaptionLines[0] = "{0} {1}".format(projectCaptionLines[0], namespaceRecord.Caption)
                        relativePathToRoot = IOUtil.RelativePath(rootDir.ResolvedPath, namespaceRecord.FileDirectoryEx)
                        projectCaptionLines.append("")
                        projectCaptionLines.append("To [main document]({0}/README.md)".format(relativePathToRoot))

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
                    config.LogPrintWarning("No README.md found at {0}".format(namespaceRecord.FilePath))

            if not localToolConfig.NoMdScan:
                mdFiles = IOUtil.FindFileByExtension(rootDir.ResolvedPath, ".md", minimalConfig.IgnoreDirectories)
                for filename in mdFiles:
                    if filename != readmePath:
                        config.LogPrintVerbose(1, "Processing file '{0}'".format(filename))
                        self.ProcessMDFile(config, filename, localToolConfig)

            scrFilesCandidates = IOUtil.GetFilesAt(rootDir.ResolvedPath, True)
            for scrFilePath in scrFilesCandidates:
                if scrFilePath.endswith(".txt") and IOUtil.GetFileName(scrFilePath).startswith("SCR-"):
                    self.ProcessSCRFile(config, projectContext, scrFilePath);


    def ProcessMDFile(self, config: Config, filename: str, localToolConfig: LocalToolConfig) -> None:
        packageReadMeLines = TryLoadReadMe(filename)
        if packageReadMeLines is not None and self.HasLineThatContain(packageReadMeLines, "#AG_TOC_BEGIN#") and not IsNamespaceRootMDFile(packageReadMeLines):
            packageReadMeLinesNew = TryInsertTableOfContents(config, packageReadMeLines, localToolConfig.ToCDepth, filename)
            if packageReadMeLinesNew is not None:
                packageReadMeLines = packageReadMeLinesNew

            SaveReadMe(config, filename, packageReadMeLines)


    def HasLineThatContain(self, lines: List[str], strFind: str) -> bool:
        for entry in lines:
            if strFind in entry:
                return True
        return False

class ToolAppFlowFactory(AToolAppFlowFactory):
    #def __init__(self) -> None:
    #    pass


    def GetTitle(self) -> str:
        return 'FslBuildDoc'


    def GetToolCommonArgConfig(self) -> ToolCommonArgConfig:
        argConfig = ToolCommonArgConfig()
        argConfig.AddPlatformArg = True
        argConfig.AddGeneratorSelection = True
        argConfig.SupportBuildTime = True
        #argConfig.AllowVSVersion = True
        # These are used when: --ExtractArguments is enabled
        argConfig.AddBuildFiltering = True
        argConfig.AddBuildThreads = True
        argConfig.AddBuildVariants = True
        argConfig.AllowRecursive = True
        return argConfig


    def AddCustomArguments(self, parser: argparse.ArgumentParser, toolConfig: ToolConfig, userTag: Optional[object]) -> None:
        parser.add_argument('--DryRun', action='store_true', help='No files will be created')
        parser.add_argument('--ToCDepth', default=str(DefaultValue.ToCDepth), help='The headline depth to include, defaults to {0} (1-4)'.format(DefaultValue.ToCDepth))
        parser.add_argument('--ExtractArguments', default=DefaultValue.ExtractArguments, help='Build the app and execute it to extract the command line arguments ("*" for all or "." for current dirs package)')
        parser.add_argument('--NoMdScan', action='store_true', help='Dont scan for all "md" files and generate a table of content')

    def Create(self, toolAppContext: ToolAppContext) -> AToolAppFlow:
        return ToolFlowBuildDoc(toolAppContext)
