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

from FslBuildGen.DataTypes import AccessType, ExternalDependencyType, IncludePriority
from FslBuildGen.Log import Log
from FslBuildGen.PackageIncludeDir import PackageIncludeDir
from FslBuildGen.SemanticVersion2 import SemanticVersion2
from FslBuildGen.SemanticVersionPattern import SemanticVersionPattern
from FslBuildGen.Xml import FakeXmlElementFactory
from FslBuildGen.Xml.Exceptions import XmlException, XmlFormatException
from FslBuildGen.Xml.XmlBase import XmlBase
from FslBuildGen.Xml.XmlGenFileExternalDependencyPackageManager import XmlGenFileExternalDependencyPackageManager


class XmlGenFileExternalDependency(XmlBase):
    __AttribName = "Name"
    __AttribDebugName = "DebugName"
    __AttribTargetName = "TargetName"
    __AttribInclude = "Include"
    __AttribOverrideIncludePriority = "OverrideIncludePriority"
    __AttribLocation = "Location"
    __AttribHintPath = "HintPath"
    __AttribVersion = "Version"
    __AttribPublicKeyToken = "PublicKeyToken"
    __AttribProcessorArchitecture = "ProcessorArchitecture"
    __AttribCulture = "Culture"
    __AttribIf = "If"
    __AttribAccess = "Access"
    __AttribType = "Type"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes(
            {
                self.__AttribName,
                self.__AttribDebugName,
                self.__AttribTargetName,
                self.__AttribInclude,
                self.__AttribOverrideIncludePriority,
                self.__AttribLocation,
                self.__AttribHintPath,
                self.__AttribVersion,
                self.__AttribPublicKeyToken,
                self.__AttribProcessorArchitecture,
                self.__AttribCulture,
                self.__AttribIf,
                self.__AttribAccess,
                self.__AttribType,
            }
        )
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)
        self.DebugName: str = self._ReadAttrib(xmlElement, self.__AttribDebugName, self.Name)
        defaultTargetName = f"{self.Name}::{self.Name}"
        self.TargetName: str = self._ReadAttrib(xmlElement, self.__AttribTargetName, defaultTargetName)
        strIncludeDir: str | None = self._TryReadAttrib(xmlElement, self.__AttribInclude)
        includePriority = self._ReadIncludePriorityAttrib(xmlElement, self.__AttribOverrideIncludePriority, IncludePriority.After)

        self.Location: str | None = self._TryReadAttrib(xmlElement, self.__AttribLocation)
        # New assembly keywords primarily used for C# assemblies
        self.HintPath: str | None = self._TryReadAttrib(xmlElement, self.__AttribHintPath)
        self.Version: SemanticVersionPattern | None = self._TryReadAttribAsSemanticVersionPattern(xmlElement, self.__AttribVersion)
        self.PublicKeyToken: str | None = self._TryReadAttrib(xmlElement, self.__AttribPublicKeyToken)
        self.ProcessorArchitecture: str | None = self._TryReadAttrib(xmlElement, self.__AttribProcessorArchitecture)
        self.Culture: str | None = self._TryReadAttrib(xmlElement, self.__AttribCulture)
        self.PackageManager = self.__TryGetPackageManager(log, xmlElement)
        self.IfCondition: str | None = self._TryReadAttrib(xmlElement, self.__AttribIf)
        # Can only be set from code, and it indicates that this dependency is managed by a recipe or similar
        self.IsManaged: bool = False
        strAccess: str | None = self._TryReadAttrib(xmlElement, self.__AttribAccess)

        access = None
        if strIncludeDir is not None or strAccess is not None:
            strAccess = self._ReadAttrib(xmlElement, self.__AttribAccess) if access is None else access
            if strAccess == "Public":
                access = AccessType.Public
            elif strAccess == "Private":
                access = AccessType.Private
            else:
                raise XmlFormatException(f"Unknown access type '{access}' on external dependency: '{self.Name}'")
        self.IncludeDir = PackageIncludeDir(strIncludeDir, includePriority) if strIncludeDir is not None else None

        strElementType = self._ReadAttrib(xmlElement, self.__AttribType)
        elementType = ExternalDependencyType.TryFromString(strElementType)
        if elementType is None:
            raise XmlException(xmlElement, f"Unknown external dependency type: '{strElementType}' expected: {ExternalDependencyType.AllStrings()}")
        self.Type: ExternalDependencyType = elementType

        # The access type is only relevant for the include file location
        # the rest should always be included
        self.Access: AccessType = AccessType.Public if access is None else access
        self.ConsumedBy = None

        if self.Type == ExternalDependencyType.DLL:
            if self.IncludeDir is not None:
                raise XmlException(xmlElement, f"DLL dependency: '{self.Name}' can not contain include paths")
            if self.Access != AccessType.Public:
                raise XmlException(xmlElement, f"DLL dependency: '{self.Name}' can only have a access type of Public")

        if not isinstance(self.Access, AccessType):
            raise Exception("Internal error")

    def __TryGetPackageManager(self, log: Log, xmlElement: ET.Element) -> XmlGenFileExternalDependencyPackageManager | None:
        packageManager = None
        for child in xmlElement:
            if child.tag == "PackageManager":
                if packageManager is not None:
                    raise Exception("PackageManager has already been defined")
                packageManager = XmlGenFileExternalDependencyPackageManager(self.Log, child)
        return packageManager


class FakeXmlGenFileExternalDependency(XmlGenFileExternalDependency):
    def __init__(
        self,
        log: Log,
        name: str,
        location: str,
        access: AccessType,
        extDepType: ExternalDependencyType,
        debugName: str | None = None,
        includeLocation: str | None = None,
        isManaged: bool = False,
    ) -> None:
        strType = ExternalDependencyType.ToString(extDepType)
        fakeXmlElementAttribs: dict[str, str] = {"Name": name, "Location": location, "Access": AccessType.ToString(access), "Type": strType}

        if debugName is not None:
            fakeXmlElementAttribs["DebugName"] = debugName
        if includeLocation is not None:
            fakeXmlElementAttribs["Include"] = location

        fakeXmlElement = FakeXmlElementFactory.Create("FakeExternalDep", fakeXmlElementAttribs)
        super().__init__(log, fakeXmlElement)
        if self.Name != name:
            raise Exception("Failed to setting fake element attribute Name")
        if self.Location != location:
            raise Exception("Failed to setting fake element attribute Location")
        if self.Access != access:
            raise Exception("Failed to setting fake element attribute Access")
        if debugName is not None and self.DebugName != debugName:
            raise Exception("Failed to setting fake element attribute DebugName")
        if includeLocation is not None and (self.IncludeDir is None or self.IncludeDir.Name != includeLocation):
            raise Exception("Failed to setting fake element attribute IncludeLocation")
        # Override the value set in the base class
        self.IsManaged = isManaged


class FakeXmlGenFileExternalDependencyHeaders(FakeXmlGenFileExternalDependency):
    def __init__(self, log: Log, name: str, location: str, access: AccessType, isManaged: bool) -> None:
        super().__init__(log, name, location, access, ExternalDependencyType.Headers, None, location, isManaged=isManaged)


class FakeXmlGenFileExternalDependencyStaticLib(FakeXmlGenFileExternalDependency):
    def __init__(self, log: Log, name: str, debugName: str, location: str, access: AccessType, isManaged: bool) -> None:
        super().__init__(log, name, location, access, ExternalDependencyType.StaticLib, debugName, isManaged=isManaged)


class FakeXmlGenFileExternalDependencyDLL(FakeXmlGenFileExternalDependency):
    def __init__(self, log: Log, name: str, debugName: str, location: str, access: AccessType, isManaged: bool) -> None:
        super().__init__(log, name, location, access, ExternalDependencyType.DLL, debugName, isManaged=isManaged)


class FakeXmlGenFileExternalDependencyCMakeFindModern(XmlGenFileExternalDependency):
    def __init__(self, log: Log, name: str, version: SemanticVersion2 | None, targetName: str | None, path: str | None, ifCondition: str | None) -> None:
        fakeXmlElementAttribs = {"Name": name, "Type": ExternalDependencyType.ToString(ExternalDependencyType.CMakeFindModern)}
        if version is not None:
            fakeXmlElementAttribs["Version"] = str(version)
        if targetName is not None:
            fakeXmlElementAttribs["TargetName"] = targetName
        if path is not None:
            fakeXmlElementAttribs["Location"] = path
        if ifCondition is not None:
            fakeXmlElementAttribs["If"] = ifCondition

        fakeXmlElement = FakeXmlElementFactory.Create("FakeExternalDep", fakeXmlElementAttribs)
        super().__init__(log, fakeXmlElement)
