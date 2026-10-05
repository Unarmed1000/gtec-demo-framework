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

from FslBuildGen import Util
from FslBuildGen.Log import Log
from FslBuildGen.Xml.Exceptions import (
    XmlFlavorDefaultOptionNotSupportedException,
    XmlFlavorDefaultOptionUnknownException,
    XmlFlavorHasNoOptionsException,
    XmlFlavorOptionNameCollisionException,
    XmlFlavorUnknownElementException,
    XmlUnsupportedFlavorNameException,
    XmlUnsupportedFlavorOptionNameException,
)
from FslBuildGen.Xml.Flavor.XmlGenFileFlavorOption import XmlGenFileFlavorOption
from FslBuildGen.Xml.XmlBase import XmlBase


class XmlGenFileFlavor(XmlBase):
    __AttribName = "Name"
    __AttribQuickName = "QuickName"
    __AttribDefault = "Default"

    # The elements a flavor reads
    __ValidElements = ["Option"]

    def __init__(self, log: Log, requirementTypes: list[str], xmlElement: ET.Element, ownerPackageName: str) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribQuickName, self.__AttribDefault}, self.__ValidElements)
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)
        self.QuickName = self._TryReadAttrib(xmlElement, self.__AttribQuickName)
        # The name of the option that is used when nothing selects one, None: the flavor does not say
        self.Default = self._TryReadAttrib(xmlElement, self.__AttribDefault)
        self.IntroducedByPackageName = ownerPackageName
        # elementType = self._ReadAttrib(xmlElement, 'Type', 'Normal')
        self.Options = self.__GetXMLFlavorOptions(requirementTypes, xmlElement, ownerPackageName)
        self.OptionDict: dict[str, XmlGenFileFlavorOption] = self.__BuildOptionDict()
        self.__ValidateFlavorName()
        self.__ValidateOptionNames()
        self.__ValidateHasOptions()
        self.__ValidateDefault()

    def __ValidateFlavorName(self) -> None:
        if not Util.IsValidName(self.Name):
            raise XmlUnsupportedFlavorNameException(self.XMLElement, self.Name)

    def __ValidateHasOptions(self) -> None:
        # Every instance of the package selects one option of each of its flavors, so a flavor without options leaves the package without instances
        if len(self.Options) <= 0:
            raise XmlFlavorHasNoOptionsException(self.XMLElement, self.IntroducedByPackageName, self.Name)

    def __ValidateDefault(self) -> None:
        # The default names an option exactly as the option is written, and one the flavor itself does not mark as not supported
        if self.Default is None:
            return
        if self.Default not in self.OptionDict:
            raise XmlFlavorDefaultOptionUnknownException(
                self.XMLElement, self.IntroducedByPackageName, self.Name, self.Default, [option.Name for option in self.Options]
            )
        if not self.OptionDict[self.Default].Supported:
            raise XmlFlavorDefaultOptionNotSupportedException(self.XMLElement, self.IntroducedByPackageName, self.Name, self.Default)

    def __ValidateOptionNames(self) -> None:
        for option in self.Options:
            if not Util.IsValidName(option.Name):
                raise XmlUnsupportedFlavorOptionNameException(option.XMLElement, option.Name)

    def __BuildOptionDict(self) -> dict[str, XmlGenFileFlavorOption]:
        optionDict: dict[str, XmlGenFileFlavorOption] = {}
        optionNameSet: dict[str, str] = {}
        for option in self.Options:
            optionDict[option.Name] = option
            key = option.Name.lower()
            if key not in optionNameSet:
                optionNameSet[key] = option.Name
            else:
                raise XmlFlavorOptionNameCollisionException(self.XMLElement, optionNameSet[key], option.Name)
        return optionDict

    def __GetXMLFlavorOptions(self, requirementTypes: list[str], elem: ET.Element, ownerPackageName: str) -> list[XmlGenFileFlavorOption]:
        options = []
        if elem is not None:
            for child in elem:
                # Any other element would be ignored (a '<Define>' that was meant to be inside an option for example), so it is rejected instead
                if child.tag not in self.__ValidElements:
                    raise XmlFlavorUnknownElementException(child, ownerPackageName, self.Name, self.__ValidElements)
                options.append(XmlGenFileFlavorOption(self.Log, requirementTypes, child, ownerPackageName, self.Name))
        options.sort(key=lambda s: s.Name.lower())
        return options
