#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright 2020 NXP
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

import xml.etree.ElementTree as ET

from FslBuildGen.Log import Log
from FslBuildGen.Xml.Exceptions import XmlFlavorOptionUnknownElementException
from FslBuildGen.Xml.XmlCommonFslBuild import XmlCommonFslBuild


class XmlGenFileFlavorOption(XmlCommonFslBuild):
    __AttribName = "Name"
    __AttribSupported = "Supported"

    # The elements a option reads, 'CPPDefine' is the old name of 'Define'
    __ValidElements = ["Define", "Dependency", "ExternalDependency", "FindPackage", "Requirement"]
    __LegacyElements = ["CPPDefine"]

    def __init__(self, log: Log, requirementTypes: list[str], xmlElement: ET.Element, ownerPackageName: str, flavorName: str) -> None:
        """flavorName is the name the flavor or flavor extension element gives, it is only used in messages"""
        super().__init__(log, requirementTypes, xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribSupported})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)
        self.Supported = self._ReadBoolAttrib(xmlElement, self.__AttribSupported, True)
        self.IntroducedByPackageName = ownerPackageName
        self.DirectRequirements = self._GetXMLRequirements(xmlElement)
        # Any other element would be ignored (a '<Variant>' or '<Platform>' for example has no effect here), so it is rejected instead
        for child in xmlElement:
            if child.tag not in self.__ValidElements and child.tag not in self.__LegacyElements:
                raise XmlFlavorOptionUnknownElementException(child, ownerPackageName, flavorName, self.Name, self.__ValidElements)
