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

# The 'GenFileSchema' element of a Project.gen: where the schema versions of the gen files of a project are.
#
#   <GenFileSchema Location="https://example.com/Schema"/>     a url base
#   <GenFileSchema Location=".Config/Schema"/>                 a directory
#
# The element belongs to the project that declares it, a 'Project' or an 'ExtendedProject'. There can be one, a project without it uses
# GenFileSchema.DefaultLocation. An extended project does not get the element of its parent: its gen files are in another directory tree, so
# what the parent wrote for its own tree says nothing about it.
#
# The Location:
# - A url base ('http://' or 'https://') is kept as it is written. It never goes through the path functions, they would turn its '//' into '/'.
#   It has to name a host, and it can not hold whitespace or a control character: a gen file could not refer to such a url.
# - Anything else is a directory. It is resolved like the other paths of a Project.gen: it can start with an environment variable '$(NAME)' or
#   with '${PROJECT_ROOT}', and a relative path is relative to the directory of the Project.gen that declares it.
# - What looks like a url of another kind ('ftp://x', 'file:///x', 'http:/x') is an error and not a directory with an odd name. One letter and
#   a ':' is a Windows drive.
# Nothing is looked up in the file system: the directory does not have to exist when the Project.gen is read.

import xml.etree.ElementTree as ET

from FslBuildGen import GenFileSchema, IOUtil
from FslBuildGen.GenFileSchema import GenFileSchemaLocation
from FslBuildGen.Log import Log
from FslBuildGen.Vars.VariableEnvironment import VariableEnvironment
from FslBuildGen.Vars.VariableProcessor import VariableProcessor
from FslBuildGen.Xml.Exceptions import XmlException
from FslBuildGen.Xml.XmlBase import XmlBase

# The element, for the element lists of the readers of a project
ElementGenFileSchema = "GenFileSchema"
_g_elementName = ElementGenFileSchema
_g_projectRootVariableName = "PROJECT_ROOT"

# The characters of a URI scheme after its first letter
_g_schemeCharacters = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+.-")


def _HasScheme(value: str) -> bool:
    """True if the value starts with a URI scheme of two characters or more ('ftp:', 'file:'), one letter is a Windows drive"""
    index = value.find(":")
    return index >= 2 and value[0].isascii() and value[0].isalpha() and all(ch in _g_schemeCharacters for ch in value[1:index])


def _TryFindControlCharacter(value: str) -> str | None:
    """The first control character of the value (C0, DEL or C1), a schema reference can not hold one"""
    for ch in value:
        if ord(ch) < 0x20 or 0x7F <= ord(ch) <= 0x9F:
            return ch
    return None


def _ResolveDirectory(log: Log, value: str, projectRootDirectory: str, tag: object) -> str:
    variableEnvironment = VariableEnvironment(log)
    variableEnvironment.Set(_g_projectRootVariableName, projectRootDirectory)
    variableProcessor = VariableProcessor(log, variableEnvironment)
    environmentName = variableProcessor.TryExtractLeadingEnvironmentVariableName(value, True, tag)
    if environmentName is not None:
        # The variable processor requires the directory of an environment variable to exist, here the file system is left alone
        value = IOUtil.GetEnvironmentVariableForDirectory(environmentName, False) + value[len(environmentName) + 3 :]
    return variableProcessor.ResolvePathToAbsolute(value, tag)


class XmlGenFileSchema(XmlBase):
    __AttribLocation = "Location"

    def __init__(self, log: Log, xmlElement: ET.Element, filename: str, projectRootDirectory: str) -> None:
        """filename is the Project.gen the element is in and projectRootDirectory the directory of that file"""
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribLocation})
        value = self._ReadAttrib(xmlElement, self.__AttribLocation)
        strippedValue = value.strip()
        if len(strippedValue) <= 0:
            raise XmlException(f"The file '{filename}' has a GenFileSchema with an empty Location")
        if GenFileSchema.IsUrl(strippedValue):
            if any(ch.isspace() for ch in value):
                raise XmlException(f"The file '{filename}' has a GenFileSchema Location '{value}' that is a url with whitespace in it")
            controlCharacter = _TryFindControlCharacter(value)
            if controlCharacter is not None:
                raise XmlException(
                    f"The file '{filename}' has a GenFileSchema Location '{value}' that is a url with the control character U+{ord(controlCharacter):04X} in it"
                )
            if not GenFileSchema.HasHost(value):
                raise XmlException(f"The file '{filename}' has a GenFileSchema Location '{value}' that is a url without a host")
        elif _HasScheme(strippedValue):
            raise XmlException(f"The file '{filename}' has a GenFileSchema Location '{value}' that is not a url (http:// or https://) and not a directory")
        else:
            value = _ResolveDirectory(log, value, projectRootDirectory, xmlElement)
        self.Location = GenFileSchemaLocation(value)


def LoadGenFileSchemaLocation(log: Log, projectElement: ET.Element, filename: str, projectRootDirectory: str) -> GenFileSchemaLocation:
    """Where the schema versions of the project are: what its 'GenFileSchema' element says, the default when it has none.
    projectElement is the 'Project' or the 'ExtendedProject' element of the Project.gen 'filename', projectRootDirectory the directory of that file.
    """
    foundElements = projectElement.findall(_g_elementName)
    if len(foundElements) <= 0:
        return GenFileSchema.DefaultLocation
    if len(foundElements) > 1:
        raise XmlException(f"The file '{filename}' contained more than one {_g_elementName}")
    return XmlGenFileSchema(log, foundElements[0], filename, projectRootDirectory).Location
