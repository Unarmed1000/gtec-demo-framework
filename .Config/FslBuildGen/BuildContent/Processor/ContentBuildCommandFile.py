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

import fnmatch
import json
from typing import Any

from FslBuildGen import IOUtil, ToolSharedValues
from FslBuildGen.BuildContent.ContentRootRecord import ContentRootRecord
from FslBuildGen.BuildContent.PathVariables import PathVariables
from FslBuildGen.BuildContent.Processor.Commands import Command, CommandContentBuildSync, CommandContentSync
from FslBuildGen.BuildContent.Processor.ContentFileRecord import ContentFileRecord
from FslBuildGen.Log import Log


class ContentBuildCommandFile:
    def __init__(self, log: Log, sourceFilename: str, pathVariables: PathVariables) -> None:
        super().__init__()

        self.__RootElement = "ContentBuilder"
        self.__VersionElement = "Version"
        self.__CurrentVersion = "1"
        self.Commands: list[Command] = []

        fileContent = IOUtil.TryReadFile(sourceFilename)
        if fileContent is not None:
            log.LogPrintVerbose(3, f"Parsing command file '{sourceFilename}'")
            jsonContent = self.__ParseJsonFile(fileContent)
            self.__ValidateJsonContent(jsonContent, sourceFilename)
            self.Commands = self.__ParseContent(log, jsonContent[self.__RootElement], sourceFilename, pathVariables)

    def __ParseJsonFile(self, content: str) -> Any:
        return json.loads(content)

    def __TryReadKey(self, jsonSrcDict: Any, key: str, defaultValue: str) -> str:
        return str(jsonSrcDict[key]) if key in jsonSrcDict else defaultValue

    def __ValidateJsonContent(self, jsonContent: Any, sourceFilename: str) -> None:
        """Do some basic file format validation"""
        if len(jsonContent) != 1 or self.__RootElement not in jsonContent:
            raise Exception(f"The file '{sourceFilename}' is not of the expected format")
        childElements = jsonContent[self.__RootElement]
        if self.__VersionElement not in childElements:
            raise Exception(f"The file '{sourceFilename}' did not contain a version element")
        if childElements[self.__VersionElement] != self.__CurrentVersion:
            raise Exception(f"The file '{sourceFilename} is version {self.__VersionElement} and we expected {self.__CurrentVersion}")

    def __ParseContent(self, log: Log, jsonContent: Any, sourceFilename: str, pathVariables: PathVariables) -> list[Command]:
        commands: list[Command] = []
        for key in jsonContent:
            if key != self.__VersionElement:
                if log.Verbosity > 3:
                    log.LogPrint(f"Parsing element {key}")
                if key == "Content.Sync":
                    newCommand: Command = self.__ParseContentSync(log, jsonContent[key], sourceFilename, key, pathVariables)
                    commands.append(newCommand)
                elif key == "Content.SyncList":
                    commands += self.__ParseContentSyncList(log, jsonContent[key], sourceFilename, key, pathVariables)
                elif key == "ContentBuild.Sync":
                    newCommand = self.__ParseContentBuildSync(log, jsonContent[key], sourceFilename, key, pathVariables)
                    commands.append(newCommand)
                else:
                    raise Exception(f"Unsupported element '{key}' in file '{sourceFilename}'")

        return commands

    def __ParseContentSyncList(
        self, log: Log, jsonContent: Any, sourceFilename: str, parentElementName: str, pathVariables: PathVariables
    ) -> list[CommandContentSync]:
        commands: list[CommandContentSync] = []
        for jsonSyncEntry in jsonContent:
            newCommand = self.__ParseContentSync(log, jsonSyncEntry, sourceFilename, parentElementName, pathVariables)
            commands.append(newCommand)
        return commands

    def __ParseContentSync(self, log: Log, jsonContent: Any, sourceFilename: str, parentElementName: str, pathVariables: PathVariables) -> CommandContentSync:
        files = self.__ParseSync(log, jsonContent, sourceFilename, parentElementName, pathVariables.Content, pathVariables)
        return CommandContentSync(log, files)

    def __ParseContentBuildSync(
        self, log: Log, jsonContent: Any, sourceFilename: str, parentElementName: str, pathVariables: PathVariables
    ) -> CommandContentBuildSync:
        files = self.__ParseSync(log, jsonContent, sourceFilename, parentElementName, pathVariables.ContentBuild, pathVariables)
        return CommandContentBuildSync(log, files)

    def __ValidateContentName(self, name: str, sourceFile: str) -> None:
        invalidCharacters = "\\${}"
        for char in invalidCharacters:
            if char in name:
                raise Exception(f"'{char}' is not valid in content file names '{sourceFile}'")
        if ".." in name:
            raise Exception(f"'..' is not valid in content file names '{sourceFile}'")

    def __AppendFile(
        self,
        log: Log,
        rUniqueFiles: dict[str, ContentFileRecord],
        rFiles: list[ContentFileRecord],
        sourceRoot: ContentRootRecord,
        sourceEntry: str,
        absolutePath: str,
    ) -> None:
        contentFile = ContentFileRecord(log, sourceRoot, sourceEntry, absolutePath)
        if contentFile.Id in rUniqueFiles:
            if contentFile.ResolvedPath == rUniqueFiles[contentFile.Id].ResolvedPath:
                raise Exception(
                    f"The fileId '{contentFile.Id}' has already been added as part of '{rUniqueFiles[contentFile.Id].SourceEntry}' now trying to add is as part of '{contentFile.SourceEntry}'"
                )
            else:
                raise Exception(
                    f"The fileId '{contentFile.Id}' has already been added as part of '{rUniqueFiles[contentFile.Id].SourceEntry}' now trying to add is as part of '{contentFile.SourceEntry}' the only difference being filename casing '{rUniqueFiles[contentFile.Id].ResolvedPath}' vs '{contentFile.ResolvedPath}'"
                )
        rFiles.append(contentFile)
        rUniqueFiles[contentFile.Id] = contentFile

    def __AppendDirectory(
        self,
        log: Log,
        rUniqueFiles: dict[str, ContentFileRecord],
        rFiles: list[ContentFileRecord],
        sourceRoot: ContentRootRecord,
        sourceEntry: str,
        absolutePath: str,
    ) -> None:
        newFiles = IOUtil.GetFilePaths(absolutePath, None)
        for entry in newFiles:
            self.__AppendFile(log, rUniqueFiles, rFiles, sourceRoot, sourceEntry, entry)

    def __AppendFilesUsingWildcard(
        self,
        log: Log,
        rUniqueFiles: dict[str, ContentFileRecord],
        rFiles: list[ContentFileRecord],
        sourceRoot: ContentRootRecord,
        sourceEntry: str,
        absolutePath: str,
        directory: str,
        filename: str,
    ) -> None:
        if not IOUtil.IsDirectory(directory):
            raise Exception(f"No directory named '{directory}' found")
        candidateFiles = IOUtil.GetFilesAt(directory, False)
        for candidate in candidateFiles:
            if fnmatch.fnmatch(candidate, filename):
                absPath = IOUtil.Join(directory, candidate)
                self.__AppendFile(log, rUniqueFiles, rFiles, sourceRoot, sourceEntry, absPath)

    def __ParseSync(
        self, log: Log, jsonContent: Any, sourceFilename: str, parentElementName: str, defaultSourcePath: str, pathVariables: PathVariables
    ) -> list[ContentFileRecord]:
        sourcePath = self.__TryReadKey(jsonContent, "SourcePath", defaultSourcePath)
        sourceRoot = ContentRootRecord(log, sourcePath, pathVariables)
        contentKey = ToolSharedValues.CONTENT_FOLDER_NAME
        if contentKey not in jsonContent:
            raise Exception(f"The key '{contentKey}' was not found under '{parentElementName}' in file '{sourceFilename}")
        content = jsonContent[contentKey]
        uniqueFiles: dict[str, ContentFileRecord] = {}
        files: list[ContentFileRecord] = []
        for entry in content:
            self.__ValidateContentName(entry, sourceFilename)
            absolutePath = IOUtil.Join(sourceRoot.ResolvedPath, entry)
            directory = IOUtil.GetDirectoryName(absolutePath)
            filename = IOUtil.GetFileName(absolutePath)
            # check if there is a wild card in the filename
            if "*" in filename:
                self.__AppendFilesUsingWildcard(log, uniqueFiles, files, sourceRoot, entry, absolutePath, directory, filename)
            elif IOUtil.IsDirectory(absolutePath):
                self.__AppendDirectory(log, uniqueFiles, files, sourceRoot, entry, absolutePath)
            elif IOUtil.IsFile(absolutePath):
                self.__AppendFile(log, uniqueFiles, files, sourceRoot, entry, absolutePath)
            else:
                raise Exception(f"No file or directory found for entry '{entry}' which expands to '{absolutePath}'")
        return files
