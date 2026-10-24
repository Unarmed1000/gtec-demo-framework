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

import os
import re
import shlex
import subprocess

from FslBuildGen import IOUtil, TextFileReader
from FslBuildGen.BuildContent import ContentProfile, ContentToolTempDirectory, MakeDependencyFile
from FslBuildGen.BuildContent.ContentProcessor import ContentProcessor
from FslBuildGen.BuildContent.ContentProfile import ResolvedContentProfile
from FslBuildGen.BuildContent.ContentToolDependencyFile import g_contentToolDependencyFile
from FslBuildGen.BuildContent.PathRecord import PathRecord
from FslBuildGen.BuildContent.ToolFinder import ToolFinder
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log
from FslBuildGen.ToolConfig import ToolContentBuilder
from FslBuildGen.Xml.XmlToolConfigFile import XmlConfigContentBuilderAddExtension

# $(OutputFileName) = Absolute path to the output file
# $(InputFileName)  = Absolute path to the input file
# $(PlatformName)   = The platform the content is built for
# $(Profile)        = The name of the profile of the input file (ContentProfile)
# $(Profile.<Name>) = The value of a property of the input file: '$(Profile.Encoding)' is 'sRGB', 'Linear' or 'Normal' for a texture
# A variable can be an argument or a part of one ('-o=$(OutputFileName)')

_g_profileVariable = re.compile(r"\$\(Profile(?:\.([^)]*))?\)")


class BasicContentProcessor(ContentProcessor):
    def __init__(self, log: Log, toolFinder: ToolFinder, contentBuilderConfig: ToolContentBuilder, platformName: str | None = None) -> None:
        """platformName: the platform the content is built for, None when the caller only asks what would be built"""
        super().__init__(contentBuilderConfig.Name, contentBuilderConfig.FeatureRequirements, self.__GetExtensionsSet(contentBuilderConfig.DefaultExtensions))
        self.BasedUpon = contentBuilderConfig
        self.DefaultExtensions = contentBuilderConfig.DefaultExtensions
        self.ToolArguments = self.__ProcessToolParameters(contentBuilderConfig.Parameters)
        self.ToolCommand = toolFinder.GetPlatformDependentExecuteableName(contentBuilderConfig.Executable)
        self.ToolDescription = contentBuilderConfig.Description
        self.PlatformNames = contentBuilderConfig.PlatformNames
        self.__PlatformName = platformName
        self.__DependencyFileOption = contentBuilderConfig.DependencyFileOption
        self.__ToolFinder = toolFinder

    def __GetExtensionsSet(self, extensions: list[XmlConfigContentBuilderAddExtension]) -> set[str]:
        return {extension.Name for extension in extensions}

    def __ProcessToolParameters(self, toolParameters: str) -> list[str]:
        res = shlex.split(toolParameters)
        return res

    def GetOutputFileName(self, log: Log, contentOutputPath: str, contentFileRecord: PathRecord, removeExtension: bool = False) -> str:
        outputFilename = super().GetOutputFileName(log, contentOutputPath, contentFileRecord, removeExtension)
        extension = self.__TryFindExtension(contentFileRecord.RelativePath)
        if extension is None or extension.PostfixedOutputExtension is None or len(extension.PostfixedOutputExtension) <= 0:
            return outputFilename
        return outputFilename + extension.PostfixedOutputExtension

    def __TryFindExtension(self, contentFile: str) -> XmlConfigContentBuilderAddExtension | None:
        for entry in self.DefaultExtensions:
            if contentFile.endswith(entry.Name):
                return entry
        return None

    def CheckTool(self) -> None:
        """Stops if the tool of the builder is not there: told before a build starts, not at the first file that needs it"""
        self.__ToolFinder.CheckToolCommand(self.ToolCommand, self.ToolDescription)

    def __GetToolParameterList(self, toolArguments: list[str], outputFile: str, inputFile: str, profile: ResolvedContentProfile | None) -> list[str]:
        res = []
        for argument in toolArguments:
            argument = argument.replace("$(OutputFileName)", outputFile).replace("$(InputFileName)", inputFile)
            if "$(PlatformName)" in argument:
                if self.__PlatformName is None:
                    raise Exception(f"The ContentBuilder '{self.Name}' uses $(PlatformName), so it can not be run without the platform it builds for")
                argument = argument.replace("$(PlatformName)", self.__PlatformName)
            argument = _g_profileVariable.sub(lambda match: self.__GetProfileValue(match, inputFile, profile), argument)
            res.append(argument)
        return res

    def __GetProfileValue(self, match: re.Match[str], inputFile: str, profile: ResolvedContentProfile | None) -> str:
        if profile is None:
            raise UsageErrorException(
                f"The ContentBuilder '{self.Name}' uses {match.group(0)}, so it can only build a file of a content directory: '{inputFile}' has no profile"
            )
        name = match.group(1)
        if name is None:
            return profile.ProfileName
        value = profile.Properties.get(name)
        if value is None:
            properties = ", ".join(ContentProfile.GetPropertyValues(profile.ContentType)) or "none"
            raise UsageErrorException(
                f"The ContentBuilder '{self.Name}' uses {match.group(0)} for '{inputFile}', a {profile.ContentType.value.lower()} file, which has no "
                f"property '{name}'. Its properties are: {properties}"
            )
        return value

    def Process(
        self,
        log: Log,
        configDisableWrite: bool,
        contentBuildPath: str,
        contentOutputPath: str,
        contentFileRecord: PathRecord,
        outputFileName: str | None = None,
        toolArguments: list[str] | None = None,
        profile: ResolvedContentProfile | None = None,
    ) -> list[str] | None:
        """outputFileName: the output of the build step (a rule can name it, a namespace moves it), None for the output the builder names
        toolArguments: the parameters a build rule gives the builder, None for its own
        profile: what the input file is, for the '$(Profile...)' variables. None for a file that is not of a content directory
        """
        usedToolArguments = self.ToolArguments if toolArguments is None else toolArguments
        if configDisableWrite:
            # Nothing is written, so the tool is not run: its parameters and the tool itself are checked, which a run would have done
            self.__GetToolParameterList(usedToolArguments, "", contentFileRecord.ResolvedPath, profile)
            self.__ToolFinder.CheckToolCommand(self.ToolCommand, self.ToolDescription)
            return None

        if outputFileName is None:
            outputFileName = self.GetOutputFileName(log, contentOutputPath, contentFileRecord)
        self.EnsureDirectoryExist(configDisableWrite, outputFileName)

        # The tool writes to a temporary file, so the output file is only modified if the tool made something. The file is in a
        # directory of this run alone: the same source file can be built for several apps at the same time (ContentToolTempDirectory)
        with ContentToolTempDirectory.Create() as tempPath:
            tmpOutputFileName = self.GetTempFileName(tempPath, contentFileRecord)
            buildCommand = [self.ToolCommand]
            buildCommand += self.__GetToolParameterList(usedToolArguments, tmpOutputFileName, contentFileRecord.ResolvedPath, profile)

            # Tools that can list the files the content file includes write them to a dependency file
            tmpDependencyFileName = self.GetTempFileName(tempPath, contentFileRecord, ".d.tmp")
            dependencyArguments = g_contentToolDependencyFile.TryGetArguments(log, self.ToolCommand, tmpDependencyFileName, self.__DependencyFileOption)
            if dependencyArguments is not None:
                buildCommand += dependencyArguments

            try:
                result = subprocess.call(buildCommand, cwd=contentBuildPath)
                if result != 0:
                    self.__ToolFinder.CheckToolCommand(self.ToolCommand, self.ToolDescription)
                    raise Exception(f"{self.ToolCommand}: Failed to process file '{contentFileRecord.ResolvedPath}' ({self.ToolDescription})")
                IOUtil.CopySmallFile(tmpOutputFileName, outputFileName)
                if dependencyArguments is None:
                    return None
                return self.__ReadDependencyFile(log, tmpDependencyFileName, contentBuildPath, contentFileRecord)
            except:
                self.__ToolFinder.CheckToolCommand(self.ToolCommand, self.ToolDescription)
                raise

    def __ReadDependencyFile(self, log: Log, dependencyFileName: str, contentBuildPath: str, contentFileRecord: PathRecord) -> list[str] | None:
        """The files the tool listed in its dependency file (relative paths are relative to the directory the tool ran in)"""
        content = TextFileReader.TryReadUTF8OrLocale(log, dependencyFileName, "dependency file", skipBom=True, warn=False)
        if content is None:
            log.DoPrintWarning(f"{self.ToolCommand} did not write the dependency file for '{contentFileRecord.ResolvedPath}', so its includes are not tracked")
            return None
        dependencies = MakeDependencyFile.ParseMakeDependencyFile(content).Prerequisites
        return [IOUtil.NormalizePath(os.path.join(contentBuildPath, entry)) for entry in dependencies]
