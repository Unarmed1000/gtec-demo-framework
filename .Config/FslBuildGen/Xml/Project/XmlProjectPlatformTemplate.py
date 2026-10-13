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

# The project template of a platform, in the 'Project' of a project file:
#
#   <Project Name="Demo" DefaultTemplate=".Net10" ...>
#     <PlatformTemplate Platform="Unity" Name=".Net10Unity"/>
#
# 'DefaultTemplate' names the Visual Studio project template the projects of every platform are made from. A 'PlatformTemplate' names
# another one for the projects of one platform. A platform without one uses the default.

import xml.etree.ElementTree as ET

from FslBuildGen import PackageConfig
from FslBuildGen.Log import Log
from FslBuildGen.Xml.Exceptions import XmlException2, XmlUnsupportedPlatformException
from FslBuildGen.Xml.XmlBase import XmlBase

ElementPlatformTemplate = "PlatformTemplate"


class XmlProjectPlatformTemplate(XmlBase):
    __AttribPlatform = "Platform"
    __AttribName = "Name"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribPlatform, self.__AttribName})
        self.Platform = self._ReadAttrib(xmlElement, self.__AttribPlatform)
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)
        if self.Platform not in PackageConfig.APPROVED_PLATFORM_NAMES:
            raise XmlUnsupportedPlatformException(xmlElement, self.Platform)
        if len(self.Name) <= 0:
            raise XmlException2(f"The {ElementPlatformTemplate} of the platform '{self.Platform}' names no template")


def LoadPlatformTemplates(log: Log, projectElem: ET.Element, filename: str) -> dict[str, str]:
    """The name of the project template of each platform that has one of its own"""
    result: dict[str, str] = {}
    for foundElement in projectElem.findall(ElementPlatformTemplate):
        entry = XmlProjectPlatformTemplate(log, foundElement)
        if entry.Platform in result:
            raise XmlException2(f"The file '{filename}' contained more than one {ElementPlatformTemplate} for the platform '{entry.Platform}'")
        result[entry.Platform] = entry.Name
    return result
