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

import os
import os.path
import xml.etree.ElementTree as ET

from FslBuildGen.Exceptions import FileNotFoundException
from FslBuildGen.Log import Log
from FslBuildGen.Xml import FakeXmlElementFactory
from FslBuildGen.Xml.Exceptions import XmlException, XmlException2, XmlInvalidRootElement
from FslBuildGen.Xml.Project.XmlBuildDocConfiguration import XmlBuildDocConfiguration
from FslBuildGen.Xml.Project.XmlClangTidyConfiguration import XmlClangTidyConfiguration
from FslBuildGen.Xml.Project.XmlCMakeConfiguration import XmlCMakeConfiguration
from FslBuildGen.Xml.Project.XmlProjectRootConfigFile import (
    XmlClangFormatConfiguration,
    XmlConfigCompilerConfiguration,
    XmlConfigFileAddRootDirectory,
    XmlDotnetFormatConfiguration,
    XmlExperimental,
    XmlProjectRootConfigFile,
)
from FslBuildGen.Xml.ToolConfig import LoadUtil
from FslBuildGen.Xml.ToolConfig.XmlConfigFileAddNewProjectTemplatesRootDirectory import XmlConfigFileAddNewProjectTemplatesRootDirectory
from FslBuildGen.Xml.ToolConfig.XmlConfigPackageConfiguration import XmlConfigPackageConfiguration
from FslBuildGen.Xml.XmlBase import XmlBase


class XmlConfigFileGenFile(XmlBase):
    __AttribName = "Name"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)


class XmlConfigFileTemplateFolder(XmlBase):
    __AttribName = "Name"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)


class XmlConfigFileAddTemplateImportDirectory(XmlBase):
    __AttribName = "Name"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)


class XmlConfigContentBuilderAddExtension(XmlBase):
    __AttribName = "Name"
    __AttribDescription = "Description"
    __AttribPostfixedOutputExtension = "PostfixedOutputExtension"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribDescription, self.__AttribPostfixedOutputExtension})
        self.Name = self._ReadAttrib(xmlElement, self.__AttribName)
        self.Description = self._ReadAttrib(xmlElement, self.__AttribDescription)
        self.PostfixedOutputExtension = self._ReadAttrib(xmlElement, self.__AttribPostfixedOutputExtension, "")


class XmlConfigContentBuilder(XmlBase):
    __AttribName = "Name"
    __AttribExecutable = "Executable"
    __AttribParameters = "Parameters"
    __AttribFeatureRequirements = "FeatureRequirements"
    __AttribDescription = "Description"

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribName, self.__AttribExecutable, self.__AttribParameters, self.__AttribFeatureRequirements, self.__AttribDescription})
        self.Name: str = self._ReadAttrib(xmlElement, self.__AttribName)
        self.Executable: str = self._ReadAttrib(xmlElement, self.__AttribExecutable)
        self.Parameters: str = self._ReadAttrib(xmlElement, self.__AttribParameters)
        self.FeatureRequirements: str = self._ReadAttrib(xmlElement, self.__AttribFeatureRequirements, "")
        self.DefaultExtensions: list[XmlConfigContentBuilderAddExtension] = self.__LoadDefaultExtensions(log, xmlElement)
        self.Description: str = self._ReadAttrib(xmlElement, self.__AttribDescription)

    def __LoadDefaultExtensions(self, log: Log, xmlElement: ET.Element) -> list[XmlConfigContentBuilderAddExtension]:
        res = []
        foundElements = xmlElement.findall("AddExtension")
        for foundElement in foundElements:
            res.append(XmlConfigContentBuilderAddExtension(log, foundElement))
        return res


class XmlConfigContentBuilderConfiguration(XmlBase):
    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes(set())
        self.ContentBuilders: list[XmlConfigContentBuilder] = self.__LoadContentBuilders(log, xmlElement)

    def __LoadContentBuilders(self, log: Log, xmlElement: ET.Element) -> list[XmlConfigContentBuilder]:
        res = []
        foundElements = xmlElement.findall("ContentBuilder")
        for foundElement in foundElements:
            res.append(XmlConfigContentBuilder(log, foundElement))
        return res


class FakeXmlConfigContentBuilderConfiguration(XmlConfigContentBuilderConfiguration):
    def __init__(self, log: Log) -> None:
        xmlElement = FakeXmlElementFactory.Create("Config")
        super().__init__(log, xmlElement)


class XmlToolConfigFile(XmlBase):
    __AttribVersion = "Version"

    def __init__(self, log: Log, filename: str, projectRootConfig: XmlProjectRootConfigFile) -> None:
        if projectRootConfig is None:
            raise Exception("projectRootConfig can not be None")
        if not os.path.isfile(filename):
            raise FileNotFoundException("Could not locate config file %s", filename)

        tree = ET.parse(filename)
        elem = tree.getroot()
        if elem.tag != "FslBuildGenConfig":
            raise XmlInvalidRootElement("The file did not contain the expected root tag 'FslBuildGenConfig'")

        super().__init__(log, elem)
        # self._CheckAttributes({self.__AttribVersion})
        currentVersion = "2"
        fileVersion = self._ReadAttrib(elem, self.__AttribVersion)
        if fileVersion != currentVersion:
            raise XmlException(f"The file was not of the expected version {currentVersion}")

        # In V2 we do not support local AddRootDirectory elements, we use the ones in ProjectRootConfig
        rootDirs = projectRootConfig.XmlRootDirectories
        if len(rootDirs) < 1:
            raise XmlException("The file did not contain at least one AddRootDirectory element")

        templateImportDirectory = self.__LoadAddTemplateImportDirectory(elem)

        self.__CheckForLegacyElements(elem, filename)

        # In V2 we do not support local PackageConfiguration elements, we use the ones in ProjectRootConfig
        xmlPackageConfigurations: list[XmlConfigPackageConfiguration] = projectRootConfig.XmlPackageConfiguration
        if len(xmlPackageConfigurations) < 1:
            if projectRootConfig.SourceFileName is None:
                raise XmlException(f"The file '{filename}' did not contain at least one PackageConfiguration element")
            else:
                raise XmlException(f"The file '{filename}' and {projectRootConfig.SourceFileName} did not contain at least one PackageConfiguration element")

        newProjectTemplatesRootDirectories = LoadUtil.LoadAddNewProjectTemplatesRootDirectory(log, elem, filename)
        newProjectTemplatesRootDirectories = self.__MergeNewProjectTemplatesRootDirectories(
            newProjectTemplatesRootDirectories, projectRootConfig.XmlNewProjectTemplatesRootDirectories
        )

        xmlContentBuilderConfiguration = self.__LoadContentBuilderConfiguration(elem)

        xmlConfigFileTemplateFolder = self.__LoadTemplateFolder(elem)

        self.Version: int = int(fileVersion)
        self.RootDirectories: list[XmlConfigFileAddRootDirectory] = rootDirs
        self.TemplateImportDirectories: list[XmlConfigFileAddTemplateImportDirectory] = templateImportDirectory
        self.PackageConfiguration: dict[str, XmlConfigPackageConfiguration] = self.__ResolvePackageConfiguration(xmlPackageConfigurations)
        self.NewProjectTemplateRootDirectories: list[XmlConfigFileAddNewProjectTemplatesRootDirectory] = newProjectTemplatesRootDirectories
        self.TemplateFolder: XmlConfigFileTemplateFolder = xmlConfigFileTemplateFolder
        self.GenFileName: XmlConfigFileGenFile = self.__LoadGenFileName(elem)
        self.ContentBuilderConfiguration: XmlConfigContentBuilderConfiguration = xmlContentBuilderConfiguration
        self.BuildDocConfiguration: list[XmlBuildDocConfiguration] = projectRootConfig.XmlBuildDocConfiguration
        self.ClangFormatConfiguration: list[XmlClangFormatConfiguration] = projectRootConfig.XmlClangFormatConfiguration
        self.DotnetFormatConfiguration: list[XmlDotnetFormatConfiguration] = projectRootConfig.XmlDotnetFormatConfiguration
        self.ClangTidyConfiguration: list[XmlClangTidyConfiguration] = projectRootConfig.XmlClangTidyConfiguration
        self.CMakeConfiguration: list[XmlCMakeConfiguration] = projectRootConfig.XmlCMakeConfiguration
        self.CompilerConfiguration: list[XmlConfigCompilerConfiguration] = projectRootConfig.XmlCompilerConfiguration
        self.Experimental: XmlExperimental | None = self.__ResolveExperimental(projectRootConfig.XmlExperimental)

    def __MergeNewProjectTemplatesRootDirectories(
        self,
        newProjectTemplatesRootDirectories1: list[XmlConfigFileAddNewProjectTemplatesRootDirectory],
        newProjectTemplatesRootDirectories2: list[XmlConfigFileAddNewProjectTemplatesRootDirectory],
    ) -> list[XmlConfigFileAddNewProjectTemplatesRootDirectory]:
        uniqueDict: dict[str, XmlConfigFileAddNewProjectTemplatesRootDirectory] = {}
        result: list[XmlConfigFileAddNewProjectTemplatesRootDirectory] = []

        self.__AddNewProjectTemplatesRootDirectories(result, uniqueDict, newProjectTemplatesRootDirectories1)
        self.__AddNewProjectTemplatesRootDirectories(result, uniqueDict, newProjectTemplatesRootDirectories2)
        return result

    def __AddNewProjectTemplatesRootDirectories(
        self,
        rDst: list[XmlConfigFileAddNewProjectTemplatesRootDirectory],
        rDstUniqueDict: dict[str, XmlConfigFileAddNewProjectTemplatesRootDirectory],
        src: list[XmlConfigFileAddNewProjectTemplatesRootDirectory],
    ) -> None:
        for entry in src:
            if entry.Id in rDstUniqueDict:
                raise Exception(f"Duplicated template root with the id {entry.Id} at {rDstUniqueDict[entry.Id].SourceFileName} and {entry.SourceFileName}")
            rDstUniqueDict[entry.Id] = entry
            rDst.append(entry)

    def __CheckForLegacyElements(self, xmlElement: ET.Element, filename: str) -> None:
        self.__CheckForLegacyElement(xmlElement, filename, "PackageConfiguration")
        self.__CheckForLegacyElement(xmlElement, filename, "AddRootDirectory")

    def __CheckForLegacyElement(self, xmlElement: ET.Element, filename: str, name: str) -> None:
        found = xmlElement.find(name)
        if found is not None:
            raise XmlException(f"The file '{filename}' contained a legacy {name} element which is not supported anymore. Use the 'Project.gen' file instead")

    def __ResolvePackageConfiguration(self, xmlPackageConfigurations: list[XmlConfigPackageConfiguration]) -> dict[str, XmlConfigPackageConfiguration]:
        # prepare the package configurations
        packageConfigurationDict: dict[str, XmlConfigPackageConfiguration] = {}
        for entry in xmlPackageConfigurations:
            if entry.Name in packageConfigurationDict:
                firstEntry = packageConfigurationDict[entry.Name]
                raise XmlException2(f"Duplicated package configuration name '{entry.Name}' found in '{entry.SourceFile}' and '{firstEntry.SourceFile}'")
            packageConfigurationDict[entry.Name] = entry

        if "default" not in packageConfigurationDict:
            raise XmlException("The file did not contain the 'default' PackageConfiguration element")
        return packageConfigurationDict

    def __ResolveExperimental(self, xmlExperimental: XmlExperimental | None) -> XmlExperimental | None:
        return xmlExperimental

    def __LoadTemplateFolder(self, xmlElement: ET.Element) -> XmlConfigFileTemplateFolder:
        foundElement = xmlElement.find("TemplateFolder")
        if foundElement is None:
            raise XmlException2("Could not locate the TemplateFolder element")
        return XmlConfigFileTemplateFolder(self.Log, foundElement)

    def __LoadGenFileName(self, xmlElement: ET.Element) -> XmlConfigFileGenFile:
        foundElement = xmlElement.find("GenFile")
        if foundElement is None:
            raise XmlException2("Could not locate the GenFile element")
        return XmlConfigFileGenFile(self.Log, foundElement)

    def __LoadAddTemplateImportDirectory(self, xmlElement: ET.Element) -> list[XmlConfigFileAddTemplateImportDirectory]:
        res = []
        foundElements = xmlElement.findall("AddTemplateImportDirectory")
        for foundElement in foundElements:
            res.append(XmlConfigFileAddTemplateImportDirectory(self.Log, foundElement))
        return res

    def __LoadContentBuilderConfiguration(self, xmlElement: ET.Element) -> XmlConfigContentBuilderConfiguration:
        foundElements = xmlElement.findall("ContentBuilderConfiguration")
        if len(foundElements) > 1:
            raise XmlException("The file contained more than one ContentBuilderConfiguration")
        elif len(foundElements) == 1:
            return XmlConfigContentBuilderConfiguration(self.Log, foundElements[0])
        return FakeXmlConfigContentBuilderConfiguration(self.Log)
