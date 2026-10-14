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

# A file that a <Generate> embeds in the file it generates:
#
#   <Generate Template="template/Shaders.cpp.txt" Target="source/Lib/Shaders.cpp">
#     <EmbedFile Name="SHADER_FRAG" File="OriginalContent/Shader.frag" Builder="VulkanShaderCompiler" Format="UInt32"/>
#   </Generate>
#
# What the names mean is said where they are used (GenerateEmbeddedFiles). Here the element is read and what can be told from the element
# alone is checked.

import re
import xml.etree.ElementTree as ET

from FslBuildGen.EmbedFileFormat import EmbedFileFormat
from FslBuildGen.Log import Log
from FslBuildGen.Xml.Exceptions import XmlFormatException
from FslBuildGen.Xml.XmlBase import XmlBase

ElementName = "EmbedFile"

_g_validName = re.compile("^[A-Z][A-Z0-9_]*$")


class XmlGenFileEmbedFile(XmlBase):
    __AttribName = "Name"
    __AttribFile = "File"
    __AttribBuilder = "Builder"
    __AttribFormat = "Format"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribFile, self.__AttribBuilder, self.__AttribFormat})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)
        self.File = self._ReadAttrib(xmlElement, self.__AttribFile)
        self.Builder: str | None = self._TryReadAttrib(xmlElement, self.__AttribBuilder)
        strFormat = self._ReadAttrib(xmlElement, self.__AttribFormat, EmbedFileFormat.UInt8.value)

        if _g_validName.match(self.Name) is None:
            raise XmlFormatException(
                f"{ElementName} Name '{self.Name}' is not valid: it is the name the template has between '##', upper case letters, digits and '_', "
                "starting with a letter"
            )
        if len(self.File) <= 0 or "\\" in self.File or self.File.startswith("/") or ":" in self.File or ".." in self.File.split("/"):
            raise XmlFormatException(
                f"{ElementName} '{self.Name}' File '{self.File}' is not valid: it is a file of the package, by its path below the package directory "
                "with '/' between its parts"
            )
        if self.Builder is not None and len(self.Builder) <= 0:
            raise XmlFormatException(f"{ElementName} '{self.Name}' has an empty Builder: leave the attribute out to embed the file as it is")
        fileFormat = EmbedFileFormat.TryFromString(strFormat)
        if fileFormat is None:
            raise XmlFormatException(f"{ElementName} '{self.Name}' Format '{strFormat}' is not one of: {', '.join(EmbedFileFormat.GetNames())}")
        self.Format = fileFormat
