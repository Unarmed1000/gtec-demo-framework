#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright (c) 2014 Freescale Semiconductor, Inc.
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

import xml.etree.ElementTree as ET

from FslBuildGen import Util
from FslBuildGen.DataTypes import AccessType, DependencyOutputType
from FslBuildGen.Log import Log
from FslBuildGen.Xml.Exceptions import XmlException2, XmlFormatException
from FslBuildGen.Xml.XmlBase import XmlBase


class XmlGenFileDependency(XmlBase):
    __AttribName = "Name"
    __AttribFlavor = "Flavor"
    __AttribAccess = "Access"
    __AttribIf = "If"

    # These used to describe how a source generator was attached, that is now done by <SourceGeneration><Generator/></SourceGeneration>.
    # They are kept here so we can produce a error that names the replacement instead of a generic 'unknown attribute' error.
    __RemovedAttribOutputType = "OutputType"
    __RemovedAttribReferenceOutputAssembly = "ReferenceOutputAssembly"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self.__CheckRemovedAttributes(xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribFlavor, self.__AttribAccess, self.__AttribIf})
        self.Name: str = self._ReadAttrib(xmlElement, self.__AttribName)
        flavor: str | None = self._TryReadAttrib(xmlElement, self.__AttribFlavor)
        self.Flavor = self.__TryParseFlavor(flavor)
        access: str = self._ReadAttrib(xmlElement, self.__AttribAccess, "Public")
        self.IfCondition: str | None = self._TryReadAttrib(xmlElement, self.__AttribIf)

        if access == "Public":
            self.Access: AccessType = AccessType.Public
        elif access == "Private":
            self.Access = AccessType.Private
        elif access == "Link":
            self.Access = AccessType.Link
        else:
            raise XmlFormatException(f"Unknown access type '{access}' on Dependency: '{self.Name}'")

        # A dependency written in a gen file is always a plain reference, everything else is described by <SourceGeneration>
        self.OutputType = DependencyOutputType.Reference
        self.ReferenceOutputAssembly = True

    def __CheckRemovedAttributes(self, xmlElement: ET.Element) -> None:
        name = self._TryReadAttrib(xmlElement, self.__AttribName, "")
        if self._HasAttrib(xmlElement, self.__RemovedAttribOutputType):
            raise XmlException2(
                f"Dependency '{name}' uses the removed attribute '{self.__RemovedAttribOutputType}'. "
                f'Use <SourceGeneration><Generator Name="{name}"/></SourceGeneration> instead'
            )
        if self._HasAttrib(xmlElement, self.__RemovedAttribReferenceOutputAssembly):
            raise XmlException2(
                f"Dependency '{name}' uses the removed attribute '{self.__RemovedAttribReferenceOutputAssembly}'. "
                f'Use <SourceGeneration><Generator Name="{name}" Reference="true|false"/></SourceGeneration> instead'
            )

    def __TryParseFlavor(self, flavor: str | None) -> dict[str, str]:
        if flavor is None or len(flavor) <= 0:
            return {}
        uniqueIds: dict[str, str] = {}
        resDict: dict[str, str] = {}
        entries = flavor.split(",")
        for entry in entries:
            parts = entry.split("=")
            if len(parts) != 2:
                raise XmlFormatException(f"Dependency flavor constraint '{flavor}' not in the expected format 'flavor1=option, flavor2=option'")
            key = parts[0].strip()
            value = parts[1].strip()
            if key in resDict:
                raise XmlFormatException(f"Dependency flavor constraint key '{key}' already defined to '{resDict[key]}'")
            keyId = key.upper()
            if keyId in uniqueIds:
                raise XmlFormatException(f"Dependency flavor constraint key '{key}' already collides with '{uniqueIds[keyId]}'")

            if not Util.IsValidConstraintFlavorName(key):
                raise XmlFormatException(f"Dependency flavor name '{key}' is invalid")

            if not Util.IsValidFlavorOptionName(value):
                raise XmlFormatException(f"Dependency flavor option '{value}' is invalid in {key}={value}")

            uniqueIds[keyId] = key
            resDict[key] = value
        return resDict
