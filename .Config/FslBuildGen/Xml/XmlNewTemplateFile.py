#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright (c) 2016 Freescale Semiconductor, Inc.
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

import os
import os.path
import xml.etree.ElementTree as ET

from FslBuildGen import IOUtil
from FslBuildGen.Exceptions import FileNotFoundException
from FslBuildGen.Log import Log
from FslBuildGen.Xml import XmlNameCheck
from FslBuildGen.Xml.Exceptions import XmlException, XmlInvalidRootElement
from FslBuildGen.Xml.XmlBase import XmlBase

# from FslBuildGen.DataTypes import *
# from FslBuildGen.Exceptions import *
# from FslBuildGen.Xml.XmlStuff import *
# from FslBuildGen import IOUtil, Util, PackageConfig


class XmlNewTemplate(XmlBase):
    __AttribNoInclude = "NoInclude"
    __AttribForce = "Force"
    __AttribWarning = "Warning"
    __AttribStandalone = "Standalone"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribNoInclude, self.__AttribForce, self.__AttribWarning, self.__AttribStandalone})
        self.NoInclude = self._ReadBoolAttrib(xmlElement, self.__AttribNoInclude, False)
        self.Force = self._ReadBoolAttrib(xmlElement, self.__AttribForce, False)
        self.Warning = self._ReadAttrib(xmlElement, self.__AttribWarning, "")
        # False for a template that is one half of a pair: a package made from it uses a package the user makes from another
        # template, so it can not be built alone. FslBuildNew creates the package as from any template, the template sanity check
        # leaves it out of its build (NewProjectSanityCheck.py).
        self.Standalone = self._ReadBoolAttrib(xmlElement, self.__AttribStandalone, True)


class XmlNewTemplateFile(XmlBase):
    """The 'Template.xml' of a FslBuildNew template. An attribute or an element in it that is not read stops the load when the file
    is read: one error that lists every such name of the file (XmlNameCheck.XmlUnknownNamesException).
    """

    __AttribVersion = "Version"
    __ElementTemplate = "Template"

    def __init__(self, log: Log, filename: str) -> None:
        if not os.path.isfile(filename):
            raise FileNotFoundException("Could not locate config file %s", filename)

        with XmlNameCheck.ReadFile(filename):
            tree = ET.parse(filename)
            elem = tree.getroot()
            if elem.tag != "FslBuildNewTemplate":
                raise XmlInvalidRootElement("The file did not contain the expected root tag 'FslBuildGenConfig'")

            super().__init__(log, elem)
            XmlNameCheck.CheckRoot(elem, {self.__AttribVersion}, {self.__ElementTemplate})
            fileVersion = self._ReadAttrib(elem, self.__AttribVersion)
            if fileVersion != "1":
                raise Exception("The template file version was not correct")

            xmlTemplate = self.__LoadTemplateConfiguration(log, elem)

        # After the names: a 'Template' element with a typing error is reported as the unknown name it is
        if len(xmlTemplate) != 1:
            raise XmlException("The file did not contain exactly one Template element")

        self.Name = IOUtil.GetFileName(IOUtil.GetDirectoryName(filename))
        self.Id = self.Name.lower()
        self.Version: int = int(fileVersion)
        self.Template = xmlTemplate[0]
        self.Path = IOUtil.GetDirectoryName(filename)
        self.Prefix = (f"{self.Name}_").upper()

    def __LoadTemplateConfiguration(self, log: Log, element: ET.Element) -> list[XmlNewTemplate]:
        res = []
        foundElements = element.findall(self.__ElementTemplate)
        for foundElement in foundElements:
            res.append(XmlNewTemplate(log, foundElement))
        return res
