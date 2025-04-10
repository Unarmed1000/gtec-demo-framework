#!/usr/bin/env python3

#****************************************************************************************************************************************************
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
#****************************************************************************************************************************************************

from typing import List
import xml.etree.ElementTree as ET
from FslBuildGen import Util
from FslBuildGen.Log import Log
from FslBuildGen.DataTypes import PackageRequirementTypeString
from FslBuildGen.Xml.XmlBase import XmlBase
from FslBuildGen.Xml.Exceptions import XmlRequirementNameException
from FslBuildGen.Xml.Exceptions import XmlRequirementTypeException
from FslBuildGen.Xml.Exceptions import XmlRequirementStringException
from FslBuildGen.Xml.Exceptions import XmlRequirementTypeExtensionRequiresAValidExtendFieldException

class XmlGenFileRequirement(XmlBase):
    __AttribName = 'Name'
    __AttribType = 'Type'
    __AttribExtends = 'Extends'
    __AttribVersion = 'Version'

    def __init__(self, log: Log, requirementTypes: List[str], xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribType, self.__AttribExtends, self.__AttribVersion})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)        # type: str
        self.Type = self._ReadAttrib(xmlElement, self.__AttribType)        # type: str
        self.Extends = self._ReadAttrib(xmlElement, self.__AttribExtends, '') # type: str
        self.Version = self._ReadAttrib(xmlElement, self.__AttribVersion, '') # type: str

        if not Util.IsValidRequirementName(self.Name):
            raise XmlRequirementNameException(xmlElement, self.Name)
        if not self.Type in requirementTypes:
            raise XmlRequirementTypeException(xmlElement, self.Name, self.Type, self.Extends, requirementTypes)

        if len(self.Extends) > 0 and not Util.IsValidRequirementName(self.Extends):
            raise XmlRequirementStringException(xmlElement, "extends", self.Extends)
        if self.Type == PackageRequirementTypeString.Extension and len(self.Extends) == 0:
            raise XmlRequirementTypeExtensionRequiresAValidExtendFieldException(xmlElement, self.Name)
