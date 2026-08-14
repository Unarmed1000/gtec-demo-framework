#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2026, Mana Battery
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

import xml.etree.ElementTree as ET

from FslBuildGen.DataTypes import AccessType
from FslBuildGen.Log import Log
from FslBuildGen.Xml.Exceptions import XmlFormatException
from FslBuildGen.Xml.XmlBase import XmlBase


class XmlGenFileSourceGenerationGenerator(XmlBase):
    """A package that is attached to this package as a source generator.
    'Reference' controls if the generator assembly is also referenced as a normal library.
    """

    __AttribName = "Name"
    __AttribReference = "Reference"
    __AttribAccess = "Access"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribReference, self.__AttribAccess})
        self.Name: str = self._ReadAttrib(xmlElement, self.__AttribName)
        self.Reference: bool = self._ReadBoolAttrib(xmlElement, self.__AttribReference, False)
        access: str = self._ReadAttrib(xmlElement, self.__AttribAccess, "Private")

        if access == "Public":
            self.Access: AccessType = AccessType.Public
        elif access == "Private":
            self.Access = AccessType.Private
        else:
            raise XmlFormatException(f"Unknown access type '{access}' on Generator: '{self.Name}', expected: Public, Private")
