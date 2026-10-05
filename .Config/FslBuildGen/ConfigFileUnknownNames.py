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

# The names in the other xml files of the tool that it does not read (Xml/XmlNameCheck.py): every one under a directory tree.
#
# Beside the gen files the tool reads the 'Template.xml' of a FslBuildNew template, and the 'Template.xml' and 'Customization.xml' of a
# Visual Studio project template. A file that holds an attribute or an element its reader does not read stops the tool that reads it.
# This finds the files of these kinds under directory trees, reads each one with the reader the tool reads it with, without stopping at
# such a file, and gives the names of every file.
#
# - A file is found by its file name and told by its root element, so no project is resolved and no tool is run: a tree of several
#   projects, or a part of one, can be given.
# - Nothing is written.
# - A file that can not be read for another reason (what the tool would stop at too) is listed with its error, the names found in it up
#   to then are kept.
# - A file of one of the names with another root element is not a file of the tool: it is listed as skipped.
#
# The gen files of a project are listed by GenFileUnknownNames (they need their project to be loaded).

import os
import xml.etree.ElementTree as ET
from collections.abc import Callable, Collection, Sequence

from FslBuildGen import IOUtil
from FslBuildGen.Log import Log
from FslBuildGen.Xml import XmlNameCheck
from FslBuildGen.Xml.XmlNameCheck import XmlUnknownName
from FslBuildGen.Xml.XmlNewTemplateFile import XmlNewTemplateFile
from FslBuildGen.Xml.XmlNewVSProjectTemplateCustomizationFile import XmlNewVSProjectTemplateCustomizationFile
from FslBuildGen.Xml.XmlNewVSProjectTemplateFile import XmlNewVSProjectTemplateFile

# Directories no file of the tool is in
_g_skipDirectories = frozenset({".git", ".hg", ".svn", ".vs", "__pycache__", "node_modules"})


class ConfigFileKind:
    """A kind of xml file the tool reads: what a file of it is called, its root elements and the reader of it"""

    def __init__(self, name: str, fileName: str, rootTags: Collection[str], read: Callable[[Log, str], object]) -> None:
        super().__init__()
        self.Name = name
        self.FileName = fileName
        self.RootTags = frozenset(rootTags)
        self.Read = read


# The legacy root of a Visual Studio project template is read with a warning
ConfigFileKinds: Sequence[ConfigFileKind] = (
    ConfigFileKind("FslBuildNew template", "Template.xml", {"FslBuildNewTemplate"}, XmlNewTemplateFile),
    ConfigFileKind(
        "Visual Studio project template",
        "Template.xml",
        {"FslBuildGeneratorVSProjectTemplate", "FslBuildNewVSProjectTemplate"},
        XmlNewVSProjectTemplateFile,
    ),
    ConfigFileKind(
        "Visual Studio project template customization",
        "Customization.xml",
        {"FslBuildGeneratorVSProjectTemplateCustomization"},
        XmlNewVSProjectTemplateCustomizationFile,
    ),
)


class ConfigFileResult:
    """What reading one file gave"""

    def __init__(self, fileName: str, kind: ConfigFileKind, unknownNames: list[XmlUnknownName], error: str | None) -> None:
        super().__init__()
        self.FileName = fileName
        self.Kind = kind
        # In the order they are in the file
        self.UnknownNames = unknownNames
        # The error the reader stopped with, None when the file was read to its end
        self.Error = error


class ConfigFileUnknownNames:
    def __init__(self, files: list[ConfigFileResult], skipped: list[tuple[str, str]]) -> None:
        super().__init__()
        # Every file that was read, by file name
        self.Files = files
        # (file name, why): the files of one of the names that are no file of the tool
        self.Skipped = skipped

    @property
    def UnknownNames(self) -> list[XmlUnknownName]:
        """The unknown names of every file: by file, in the order they are in a file"""
        return [entry for file in self.Files for entry in file.UnknownNames]


def FindFiles(directories: Sequence[str], fileNames: Collection[str]) -> list[str]:
    """The files of one of the file names under the directories, sorted, each one once"""
    found: set[str] = set()
    for directory in directories:
        for currentDirectory, directoryNames, currentFileNames in os.walk(directory):
            directoryNames[:] = [name for name in directoryNames if name not in _g_skipDirectories]
            for fileName in currentFileNames:
                if fileName in fileNames:
                    found.add(IOUtil.NormalizePath(os.path.abspath(os.path.join(currentDirectory, fileName))))
    return sorted(found)


def _TryGetFirstTag(parser: ET.XMLPullParser[ET.Element]) -> str | None:
    """The first element the parser has seen. What it has seen before an error comes before the error."""
    try:
        for event in parser.read_events():
            element = event[-1]
            if isinstance(element, ET.Element):
                return str(element.tag)
    except ET.ParseError:
        pass
    return None


def TryGetRootTag(fileName: str) -> str | None:
    """The name of the root element of a xml file, None for a file that is no xml up to there"""
    parser = ET.XMLPullParser(events=("start",))
    try:
        with open(fileName, "rb") as file:
            while True:
                content = file.read(4096)
                if len(content) <= 0:
                    return None
                try:
                    parser.feed(content)
                    # The parser can wait for more of the file before it looks at what it has (a long comment in front of the root)
                    parser.flush()
                except ET.ParseError:
                    # A file that is broken after its root element is one for its reader, which says what is wrong with it
                    return _TryGetFirstTag(parser)
                tag = _TryGetFirstTag(parser)
                if tag is not None:
                    return tag
    except OSError:
        return None


def _ReadFile(log: Log, fileName: str, kind: ConfigFileKind) -> ConfigFileResult:
    error: str | None = None
    with XmlNameCheck.CollectUnknownNames() as collector:
        try:
            kind.Read(log, fileName)
        except Exception as ex:
            error = f"{type(ex).__name__}: {ex}"
    return ConfigFileResult(fileName, kind, collector.UnknownNames, error)


def FindUnknownNames(log: Log, directories: Sequence[str], kinds: Sequence[ConfigFileKind] = ConfigFileKinds) -> ConfigFileUnknownNames:
    """Read every file of the kinds under the directories with its reader, without stopping at a file that holds an unknown name"""
    files: list[ConfigFileResult] = []
    skipped: list[tuple[str, str]] = []
    for fileName in FindFiles(directories, {kind.FileName for kind in kinds}):
        rootTag = TryGetRootTag(fileName)
        if rootTag is None:
            skipped.append((fileName, "not a xml file"))
            continue
        baseName = IOUtil.GetFileName(fileName)
        kind = next((kind for kind in kinds if kind.FileName == baseName and rootTag in kind.RootTags), None)
        if kind is None:
            skipped.append((fileName, f"the root element '{rootTag}' is not one of the tool"))
            continue
        files.append(_ReadFile(log, fileName, kind))
    return ConfigFileUnknownNames(files, skipped)
